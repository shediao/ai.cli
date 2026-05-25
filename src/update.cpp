#include "ai/update.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <curl/curl.h>

#include <nlohmann/json.hpp>
#include <subprocess/subprocess.hpp>

#include "ai/args.h"
#include "ai/utils.h"
#include "base/temp_file.h"

namespace ai {

namespace {

// ── Platform detection ─────────────────────────────────────────────────────

/// Map the current platform to the asset target name used in releases.
/// Returns empty string when the platform is not recognised.
std::string detect_platform_target() {
#if defined(__APPLE__)
  return "darwin-universal";
#elif defined(__linux__)
#if defined(__aarch64__) || defined(__arm64__)
  return "linux-arm64";
#else
  return "linux-x64";
#endif
#elif defined(_WIN32) || defined(_WIN64)
#if defined(_M_ARM64) || defined(__aarch64__)
  return "windows-arm64";
#else
  // Detect MinGW vs MSVC – prefer the MSVC build unless we are MinGW.
#if defined(__MINGW64__) || defined(__MINGW32__)
  return "mingw64-x64";
#else
  return "windows-x64";
#endif
#endif
#elif defined(__FreeBSD__)
#if defined(__aarch64__) || defined(__arm64__)
  return "freebsd-arm64";
#else
  return "freebsd-x64";
#endif
#else
  return "";
#endif
}

// ── Version helpers ────────────────────────────────────────────────────────

struct SemVer {
  int major = 0;
  int minor = 0;
  int patch = 0;
};

/// Parse a version string ("0.1.0") into SemVer.  Returns false on failure.
bool parse_semver(const std::string& s, SemVer& out) {
  int dots = 0;
  for (char c : s) {
    if (c == '.') {
      dots++;
    }
  }
  if (dots < 1 || dots > 2) {
    return false;
  }

  std::istringstream ss(s);
  std::string token;
  int values[3] = {0, 0, 0};
  int idx = 0;
  while (std::getline(ss, token, '.') && idx < 3) {
    try {
      values[idx++] = std::stoi(token);
    } catch (...) {
      return false;
    }
  }
  out.major = values[0];
  out.minor = values[1];
  out.patch = values[2];
  return true;
}

/// Compare two SemVer values.  Returns <0 if a < b, 0 if equal, >0 if a > b.
int compare_semver(const SemVer& a, const SemVer& b) {
  if (a.major != b.major) {
    return a.major - b.major;
  }
  if (a.minor != b.minor) {
    return a.minor - b.minor;
  }
  return a.patch - b.patch;
}

/// Extract the base version from a `git describe` string.
/// "v0.1.0"       → "0.1.0"
/// "v0.1.0-5-gX"  → "0.1.0"
/// "abc123"       → "" (no tag – treat as 0.0.0)
std::string extract_base_version(const std::string& git_version) {
  if (git_version.empty()) {
    return "";
  }
  std::string v = git_version;
  // Strip leading 'v' / 'V'
  if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
    v.erase(0, 1);
  }
  // If there's a hyphen (commits after tag), take the part before it
  auto dash = v.find('-');
  if (dash != std::string::npos) {
    v = v.substr(0, dash);
  }
  // Validate that it looks like a version (contains at least one dot)
  if (v.find('.') == std::string::npos) {
    return "";  // Not a versioned build
  }
  return v;
}

// ── cURL helpers ───────────────────────────────────────────────────────────

/// Simple write callback that appends received data to a std::string.
static size_t string_write_cb(void* ptr, size_t size, size_t nmemb,
                              void* userdata) {
  auto* s = static_cast<std::string*>(userdata);
  auto total = size * nmemb;
  s->append(static_cast<const char*>(ptr), total);
  return total;
}

/// Perform an HTTPS GET request and return the response body.
/// Returns std::nullopt on any error.
std::optional<std::string> https_get(const std::string& url,
                                     std::string const& proxy) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    std::cerr << "update: curl_easy_init() failed\n";
    return std::nullopt;
  }

  std::string body;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
  // GitHub API requires a User-Agent header
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "ai-cli-updater/1.0");

  if (!proxy.empty()) {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
  }

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::cerr << "update: request failed: " << curl_easy_strerror(res) << "\n";
    return std::nullopt;
  }
  // Accept any 2xx response (redirects from GitHub API / CDN may produce
  // 200, 201, 204 etc. after following 301/302 redirects)
  if (http_code < 200 || http_code >= 300) {
    std::cerr << "update: HTTP " << http_code << " from GitHub API\n";
    return std::nullopt;
  }
  return body;
}

// ── Re-exec / replace helpers ──────────────────────────────────────────────

/// Replace the current executable with the new binary from `new_path`.
/// On Unix: rename the current binary as a backup, copy the new one in
///          place, then exec the new one.
/// On Windows: a running .exe cannot be replaced in-place.  A temporary
///             .bat script is created that waits for this process to exit,
///             moves the new binary over the old one, then relaunches.
void replace_and_restart(const std::filesystem::path& current_exe,
                         const std::filesystem::path& new_exe) {
#if defined(_WIN32)
  // Stage the new binary outside the temp directory (which gets cleaned
  // up when this process exits), right next to the current executable.
  std::filesystem::path staged = current_exe.string() + ".new";
  std::error_code ec;
  std::filesystem::remove(staged, ec);
  std::filesystem::copy_file(
      new_exe, staged, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    std::cerr << "update: failed to stage new binary: " << ec.message() << "\n";
    return;
  }

  // Create a batch file that replaces the current exe after we exit.
  std::filesystem::path bat_path = current_exe.string() + ".update.bat";
  {
    std::ofstream bat(bat_path);
    if (!bat) {
      std::cerr << "update: failed to create update batch file\n";
      return;
    }
    bat << "@echo off\r\n"
        << "REM Wait for the original process to fully exit\r\n"
        << "timeout /t 2 /nobreak > nul\r\n"
        << "move /Y \"" << staged.string() << "\" \"" << current_exe.string()
        << "\"\r\n"
        << "if errorlevel 1 (\r\n"
        << "  echo Update failed: could not replace the executable.\r\n"
        << "  echo The new binary is at: " << staged.string() << "\r\n"
        << "  pause\r\n"
        << "  exit /b 1\r\n"
        << ")\r\n"
        << "start \"\" \"" << current_exe.string() << "\"\r\n"
        << "del \"%~f0\"\r\n";
  }

  // Launch the batch file detached with no visible window
  STARTUPINFOW si;
  PROCESS_INFORMATION pi{};
  si.cb = sizeof(STARTUPINFOW);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  std::wstring cmd = L"cmd.exe /c \"" + bat_path.wstring() + L"\"";
  if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                     DETACHED_PROCESS | CREATE_NO_WINDOW, nullptr, nullptr, &si,
                     &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    // The batch script will take over – exit cleanly.
    std::exit(0);
  } else {
    std::cerr << "update: failed to launch update script (error "
              << GetLastError() << ")\n"
              << "The new binary has been staged at: " << staged << "\n"
              << "Please manually replace: " << current_exe << "\n";
  }
#else
  std::filesystem::path backup = current_exe.string() + ".old";
  std::filesystem::remove(backup);
  std::filesystem::rename(current_exe, backup);
  try {
    std::filesystem::rename(new_exe, current_exe);
  } catch (...) {
    std::filesystem::copy_file(
        new_exe, current_exe,
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(new_exe);
  }
  std::filesystem::permissions(current_exe,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_exec |
                                   std::filesystem::perms::owner_read,
                               std::filesystem::perm_options::add);
  // (Don't remove backup – keep it in case something goes wrong)

  // Exec into the new binary, passing through all original args.
  // We don't have direct access to argv/argc here, so just re-launch.
  execl(current_exe.c_str(), current_exe.c_str(), nullptr);
  // If execl fails, try execv as a fallback
  const char* argv[] = {current_exe.c_str(), nullptr};
  execv(current_exe.c_str(), const_cast<char* const*>(argv));
#endif
}

}  // namespace

// ── Public API ─────────────────────────────────────────────────────────────

int update(AiArgs const& args) {
  const std::string platform = detect_platform_target();
  if (platform.empty()) {
    std::cerr
        << "update: unsupported platform – cannot determine the right binary\n";
    return 1;
  }

  // 1. Determine current executable path
  std::filesystem::path current_exe;

#if defined(__linux__)
  {
    std::error_code ec;
    current_exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec || current_exe.empty()) {
      std::cerr << "update: cannot read /proc/self/exe\n";
      return 1;
    }
  }
#elif defined(__APPLE__)
  {
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) {
      std::cerr << "update: cannot determine executable path\n";
      return 1;
    }
    current_exe = buf;
  }
#elif defined(__FreeBSD__)
  {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    char buf[PATH_MAX];
    size_t size = sizeof(buf);
    if (sysctl(mib, 4, buf, &size, nullptr, 0) != 0) {
      std::cerr << "update: cannot determine executable path via sysctl\n";
      return 1;
    }
    current_exe = buf;
  }
#elif defined(_WIN32)
  {
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
      std::cerr << "update: cannot determine executable path\n";
      return 1;
    }
    current_exe = buf;
  }
#else
  std::cerr << "update: unsupported platform for self-update\n";
  return 1;
#endif

  current_exe = std::filesystem::absolute(current_exe);

  // 2. Fetch latest release info from GitHub
  std::cout << "Checking for updates...\n";
  auto response =
      https_get("https://api.github.com/repos/shediao/ai.cli/releases/latest",
                args.proxy.value_or(""));
  if (!response.has_value()) {
    return 1;
  }

  nlohmann::json release;
  try {
    release = nlohmann::json::parse(response.value());
  } catch (const std::exception& e) {
    std::cerr << "update: failed to parse GitHub API response: " << e.what()
              << "\n";
    return 1;
  }

  // 3. Extract latest version tag
  std::string latest_tag = release.value("tag_name", "");
  if (latest_tag.empty()) {
    std::cerr << "update: no tag_name in release JSON\n";
    return 1;
  }

  // Strip leading 'v'
  std::string latest_ver_str = latest_tag;
  if (!latest_ver_str.empty() &&
      (latest_ver_str[0] == 'v' || latest_ver_str[0] == 'V')) {
    latest_ver_str.erase(0, 1);
  }

  SemVer latest_ver;
  if (!parse_semver(latest_ver_str, latest_ver)) {
    std::cerr << "update: cannot parse latest version '" << latest_ver_str
              << "'\n";
    return 1;
  }

  // 4. Compare with current version
  const bool force = args.update_args.force;
  if (!force) {
    std::string current_base = extract_base_version(GIT_VERSION);
    SemVer current_ver;
    if (!current_base.empty() && parse_semver(current_base, current_ver)) {
      int cmp = compare_semver(current_ver, latest_ver);
      if (cmp >= 0) {
        std::cout << "Already up to date (current: " << GIT_VERSION
                  << ", latest: " << latest_tag << ")\n";
        return 0;
      }
      std::cout << "New version available: " << latest_tag
                << " (current: " << GIT_VERSION << ")\n";
    } else {
      // Could not parse current version – still offer to update
      std::cout << "New version available: " << latest_tag
                << " (current: " << (GIT_VERSION[0] ? GIT_VERSION : "unknown")
                << ")\n";
    }
  } else {
    std::cout << "Forcing update to " << latest_tag
              << " (current: " << GIT_VERSION << ")\n";
  }

  // 5. Find the right asset
#if defined(_WIN32) || defined(_WIN64)
  std::string expected_name = "ai-" + platform + ".zip";
#else
  std::string expected_name = "ai-" + platform + ".tar.gz";
#endif
  std::string download_url;
  for (const auto& asset : release["assets"]) {
    if (asset.value("name", "") == expected_name) {
      download_url = asset.value("browser_download_url", "");
      break;
    }
  }

  if (download_url.empty()) {
    std::cerr << "update: no asset found for platform '" << platform
              << "' (expected: " << expected_name << ")\n";
    return 1;
  }

  std::cout << "Downloading " << expected_name << "...\n";

  // 6. Download the archive to a temporary file
#if defined(_WIN32) || defined(_WIN64)
  ai::base::TempFile tmp_archive("ai-update.", ".zip");
#else
  ai::base::TempFile tmp_archive("ai-update.", ".tar.gz");
#endif
  std::string mime;
  if (!ai::utils::download_image(download_url, tmp_archive.path(), mime,
                                 args.proxy.value_or(""))) {
    std::cerr << "update: download failed\n";
    return 1;
  }

  // 7. Extract the binary from the archive
  ai::base::TempFile tmp_dir("ai-update-dir.", ".d");
  // Remove the temp dir path so we can create it as a directory
  std::filesystem::remove(tmp_dir.path());
  std::filesystem::create_directories(tmp_dir.path());

#if defined(_WIN32) || defined(_WIN64)
  auto [ret, _, stderr_] = subprocess::capture_run(
      {"powershell", "-NoProfile", "-Command",
       "Expand-Archive -Path '" + tmp_archive.path() + "' -DestinationPath '" +
           tmp_dir.path() + "' -Force"});
#else
  auto [ret, _, stderr_] = subprocess::capture_run(
      {"tar", "-xzf", tmp_archive.path(), "-C", tmp_dir.path()});
#endif

  if (ret != 0) {
    std::cerr << "update: failed to extract archive (exit code " << ret << ")\n"
              << stderr_.to_string() << "\n";
    return 1;
  }

  // 8. Locate the extracted binary
  std::filesystem::path new_exe;
#if defined(_WIN32)
  new_exe = std::filesystem::path(tmp_dir.path()) / "ai.exe";
#else
  new_exe = std::filesystem::path(tmp_dir.path()) / "ai";
#endif

  if (!std::filesystem::exists(new_exe)) {
    std::cerr << "update: extracted archive does not contain the binary\n";
    return 1;
  }

  std::cout << "Updating " << current_exe << " → " << latest_tag << "\n";

  replace_and_restart(current_exe, new_exe);

  // If we reach here, exec failed
  std::cerr << "update: failed to restart after update.\n"
            << "The new binary has been placed at: " << new_exe << "\n"
            << "Please manually replace: " << current_exe << "\n";
  return 1;
}

}  // namespace ai
