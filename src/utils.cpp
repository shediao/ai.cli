#include <curl/curl.h>

#include <chrono>
#include <cstdio>  // For std::remove
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>  // For std::runtime_error
#include <string>

#ifdef _WIN32
#include <windows.h>  // For GetEnvironmentVariable
#else
#include <unistd.h>  // For access, isatty, STDIN_FILENO/STDOUT_FILENO/STDERR_FILENO
#endif

#include <environment/environment.hpp>
#include <subprocess/subprocess.hpp>

#include "ai/utils.h"

namespace ai::utils {

TempFile::TempFile(std::string const& prefix, std::string const& postfix)
    : path_{getTempFilePath(prefix, postfix)} {}
TempFile::TempFile() : TempFile("", "") {}
TempFile::~TempFile() {
  if (!path_.empty() && std::filesystem::exists(path_)) {
    std::filesystem::remove(path_);
  }
}
std::string const& TempFile::path() const { return path_; }
std::optional<std::string> TempFile::content() const {
  return read_file(path_);
}

std::string getTempFilePath(std::string const& prefix,
                            std::string const& postfix) {
  std::string temp_file_path;

#ifdef _WIN32
  char temp_dir[MAX_PATH];
  if (GetTempPathA(MAX_PATH, temp_dir) == 0) {
    throw std::runtime_error("Failed to get temporary directory.");
  }

  char temp_file[MAX_PATH];
  if (GetTempFileNameA(temp_dir, prefix.c_str(), 0, temp_file) == 0) {
    throw std::runtime_error("Failed to create temporary file.");
  }
  temp_file_path = temp_file;
  DeleteFileA(temp_file);
  if (!postfix.empty()) {
    temp_file_path += postfix;
  }

#else
  std::string temp_dir = []() -> std::string {
    auto tmpdir = env::get("TMPDIR");
    if (tmpdir.has_value()) {
      return tmpdir.value();
    } else {
      return "/tmp";
    }
  }();
  std::string template_str = temp_dir;
  if (!template_str.ends_with('/')) {
    template_str.push_back('/');
  }
  if (!prefix.empty()) {
    template_str += prefix;
  }
  template_str += "XXXXXX";
  // mkstemps is a BSD extension; use standard mkstemp and append suffix
  // after generating the unique path for portability (Linux, *BSD, macOS).
  int fd = mkstemp(template_str.data());
  if (fd == -1) {
    throw std::runtime_error("Failed to create temporary file.");
  }
  close(fd);
  ::remove(template_str.c_str());
  temp_file_path = template_str;
  if (!postfix.empty()) {
    temp_file_path += postfix;
  }
#endif
  return temp_file_path;
}

TempDir::TempDir(std::string const& prefix) : path_{getTempDirPath(prefix)} {}
TempDir::TempDir() : TempDir("") {}
TempDir::~TempDir() {
  if (!path_.empty() && std::filesystem::exists(path_)) {
    // On Windows, remove_all() throws on read-only files and on symlinks
    // (especially in MSYS environments).  Reset permissions and remove
    // symlinks explicitly so removal can succeed, and use the error_code
    // overload to never throw.
#if defined(_WIN32)
    try {
      for (auto const& entry :
           std::filesystem::recursive_directory_iterator(path_)) {
        std::error_code ec;
        // Remove symlinks without following them (MSYS symlink emulation
        // can cause remove_all to fail on these).
        if (entry.is_symlink(ec) && !ec) {
          std::filesystem::remove(entry.path(), ec);
          continue;
        }
        // Reset read-only attribute so remove_all won't get "Access denied".
        auto perms = entry.status().permissions();
        if ((perms & std::filesystem::perms::owner_write) ==
            std::filesystem::perms::none) {
          std::filesystem::permissions(
              entry.path(), std::filesystem::perms::owner_write,
              std::filesystem::perm_options::add, ec);
        }
      }
    } catch (...) {
      // If iterating fails (e.g. permission denied on a subdirectory),
      // fall through and try remove_all anyway.
    }
#endif
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
}
std::string const& TempDir::path() const { return path_; }

std::string getTempDirPath(std::string const& prefix) {
  std::string temp_dir_path;

#ifdef _WIN32
  char temp_dir[MAX_PATH];
  if (GetTempPathA(MAX_PATH, temp_dir) == 0) {
    throw std::runtime_error("Failed to get temporary directory.");
  }

  // Generate a unique directory name using a temp file name as a base
  char temp_file[MAX_PATH];
  if (GetTempFileNameA(temp_dir, prefix.empty() ? "TMP" : prefix.c_str(), 0,
                       temp_file) == 0) {
    throw std::runtime_error("Failed to create temporary file name.");
  }
  // Remove the temp file and use its name (plus a suffix) as a directory name
  DeleteFileA(temp_file);
  temp_dir_path = std::string(temp_file) + ".d";
  if (!CreateDirectoryA(temp_dir_path.c_str(), nullptr)) {
    throw std::runtime_error("Failed to create temporary directory: " +
                             temp_dir_path);
  }
#else
  std::string temp_dir = []() -> std::string {
    auto tmpdir = env::get("TMPDIR");
    if (tmpdir.has_value()) {
      return tmpdir.value();
    } else {
      return "/tmp";
    }
  }();
  std::string template_str = temp_dir;
  if (!template_str.ends_with('/')) {
    template_str.push_back('/');
  }
  if (!prefix.empty()) {
    template_str += prefix;
  }
  template_str += "XXXXXX";
  // mkdtemp creates the directory and replaces XXXXXX with a unique string
  if (mkdtemp(template_str.data()) == nullptr) {
    throw std::runtime_error("Failed to create temporary directory.");
  }
  temp_dir_path = template_str;
#endif
  return temp_dir_path;
}

std::string getUserInputFromTerminal(std::string const& prompt) {
  if (!prompt.empty()) {
    std::cerr << prompt;
  }
#ifdef _WIN32
  FILE* tty = fopen("CONIN$", "r");
#else
  FILE* tty = fopen("/dev/tty", "r");
#endif
  if (!tty) {
    // Last-resort fallback: read from stdin (may be a pipe)
    std::string line;
    std::getline(std::cin, line);
    return line;
  }

  char buffer[4096]{};
  std::string line;
  if (fgets(buffer, sizeof(buffer), tty)) {
    line = buffer;
    // Strip trailing newline(s)
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
  }
  fclose(tty);
  return line;
}

std::string getUserInputViaEditor() {
  // 1. Determine the editor to use
  std::string editor;

#ifdef _WIN32
  if (auto env_editor = env::get("EDITOR"); env_editor.has_value()) {
    editor = env_editor.value();
  } else {
    // Try some common Windows editors
    editor = "notepad.exe";  // Default to Notepad
  }
#else
  if (auto env_editor = env::get("EDITOR"); env_editor.has_value()) {
    editor = env_editor.value();
  } else {
    std::vector<std::string> editors{"/usr/bin/nano", "/usr/bin/vim",
                                     "/usr/bin/vi"};
#if defined(__CYGWIN__) || defined(__MSYS__)
    for (auto& e : editors) {
      e += ".exe";
    }
#endif
    auto it = std::find_if(
        editors.begin(), editors.end(),
        [](const std::string& e) { return access(e.c_str(), X_OK) == 0; });
    if (it != editors.end()) {
      editor = *it;
    } else {
      // Default to vi or nano if available
      // If not available throw an exception.
      throw std::runtime_error(
          "No suitable editor found.  Please set the EDITOR environment "
          "variable.");
    }
  }
#endif
  // 2. Create a temporary file
  TempFile tempfile("prompt.", ".md");
  std::string temp_file_path = tempfile.path();

  // 3. Open the editor with the temporary file
  int result = subprocess::run(editor, tempfile.path());

  // Handle errors from system call (editor not found, etc.)
  if (result != 0) {
    throw std::runtime_error("Failed to execute editor: " + editor +
                             ", system return code: " + std::to_string(result));
  }

  // 4. Read the content of the temporary file
  auto user_input = tempfile.content();

  return user_input.value_or("");
}

// 回调函数，用于处理从 libcurl 接收到的数据
// ptr: 指向接收到的数据块
// size: 每个数据单元的大小（通常是1）
// nmemb: 数据单元的数量
// userdata: 用户自定义指针，这里我们将传递一个 std::ofstream*
static size_t write_data_to_file(void* ptr, size_t size, size_t nmemb,
                                 void* stream) {
  std::ofstream* out_file = static_cast<std::ofstream*>(stream);
  if (out_file && out_file->is_open()) {
    out_file->write(static_cast<char*>(ptr), size * nmemb);
    if (out_file->fail()) {
      // 写入失败，返回0会使libcurl中止传输并返回CURLE_WRITE_ERROR
      return 0;
    }
    return size * nmemb;  // 返回成功写入的字节数
  }
  return 0;  // 如果文件流无效，也中止传输
}

bool download_image(std::string const& image_url, std::string const& image_path,
                    std::string& memi_type, std::string const& proxy) {
  CURL* curl_handle;
  CURLcode res;
  std::ofstream outfile;

  // 1. 初始化 libcurl 全局环境 (通常在程序开始时调用一次)
  // 如果你的程序中多处使用 libcurl，可以考虑将全局初始化/清理放到 main
  // 函数或类构造/析构中 这里为了函数的独立性，每次都调用，但注意
  // curl_global_cleanup 也应对应调用
  // 为简单起见，假设调用者处理全局初始化/清理，或者在main中处理

  // 2. 获取一个 CURL easy handle
  curl_handle = curl_easy_init();
  if (!curl_handle) {
    std::cerr << "Error: curl_easy_init() failed." << std::endl;
    return false;
  }

  // 3. 打开本地文件用于写入 (二进制模式)
  outfile.open(image_path, std::ios::binary);
  if (!outfile.is_open()) {
    std::cerr << "Error: Cannot open file for writing: " << image_path
              << std::endl;
    curl_easy_cleanup(curl_handle);
    return false;
  }

  // 4. 设置 libcurl 选项
  // 设置要下载的 URL
  curl_easy_setopt(curl_handle, CURLOPT_URL, image_url.c_str());

  // 设置写入数据的回调函数
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_file);

  // 将文件流指针传递给回调函数
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &outfile);

  // 启用 HTTP 3xx 重定向跟随
  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

  // 在遇到 HTTP 4xx 或 5xx 错误时，让 libcurl 返回错误而不是下载错误页面
  curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);

  // (可选) 设置超时
  curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, 60000L);  // 60 秒总超时
  curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS,
                   3000L);  // 3 秒连接超时

  if (!proxy.empty()) {
    curl_easy_setopt(curl_handle, CURLOPT_PROXY, proxy.c_str());
  }

  // (可选) 详细输出，用于调试
  // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

  // 5. 执行传输
  res = curl_easy_perform(curl_handle);

  // 6. 关闭文件流 (确保所有数据都已刷入磁盘)
  outfile.close();

  // 7. 检查结果
  if (res != CURLE_OK) {
    std::cerr << "Error: curl_easy_perform() failed: "
              << curl_easy_strerror(res) << std::endl;
    // 如果下载失败，删除可能已创建的不完整文件
    std::remove(image_path.c_str());
    curl_easy_cleanup(curl_handle);
    return false;
  } else {
    // Request was successful, try to get content type
    std::string content_type_str;
    char* ct = nullptr;
    CURLcode info_res =
        curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if (info_res == CURLE_OK && ct) {
      content_type_str = ct;
      // The content_type string might have extra info like ";
      // charset=UTF-8" We might want to strip that, e.g., find the first
      // ';'
      auto image_dash_pos = content_type_str.find("image/");
      if (image_dash_pos != std::string::npos) {
        content_type_str = content_type_str.substr(image_dash_pos);
      }
      size_t semi_colon_pos = content_type_str.find(';');
      if (semi_colon_pos != std::string::npos) {
        content_type_str = content_type_str.substr(0, semi_colon_pos);
      }
      memi_type = content_type_str;
    } else {
      std::cerr << "Warning: Could not get content type. "
                << curl_easy_strerror(info_res) << std::endl;
      // content_type_str will remain empty or you could set a default
    }
  }

  long http_code = 0;
  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
  if (http_code >= 400) {  // CURLOPT_FAILONERROR
                           // 应该已经处理了大部分，但多一层检查无妨
    std::cerr << "Error: HTTP request failed with code " << http_code
              << std::endl;
    std::remove(image_path.c_str());
    curl_easy_cleanup(curl_handle);
    return false;
  }

  // 8. 清理 CURL easy handle
  curl_easy_cleanup(curl_handle);

  return true;
}

// Helper function to trim leading/trailing whitespace
static std::string trim(const std::string& str) {
  size_t first = str.find_first_not_of(" \t\n\r\f\v");
  if (std::string::npos == first) {
    return str;
  }
  size_t last = str.find_last_not_of(" \t\n\r\f\v");
  return str.substr(first, (last - first + 1));
}

// Callback function to process received headers
// It specifically looks for the Content-Type header
static size_t header_callback(char* buffer, size_t size, size_t nitems,
                              void* userdata) {
  size_t total_size = size * nitems;
  std::string header_line(buffer, total_size);
  std::string* content_type_ptr = static_cast<std::string*>(userdata);

  // Convert header name to lowercase for case-insensitive comparison
  std::string header_name;
  size_t colon_pos = header_line.find(':');
  if (colon_pos != std::string::npos) {
    header_name = header_line.substr(0, colon_pos);
    std::transform(header_name.begin(), header_name.end(), header_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (header_name == "content-type") {
      std::string value = header_line.substr(colon_pos + 1);
      // Trim whitespace and potential extra info (like charset)
      value = trim(value);
      // Sometimes content-type includes charset, e.g., "image/jpeg;
      // charset=utf-8"
      // We only want the MIME type part.
      size_t semi_colon_pos = value.find(';');
      if (semi_colon_pos != std::string::npos) {
        value = value.substr(0, semi_colon_pos);
        value = trim(value);  // Trim again after potentially removing charset
      }
      *content_type_ptr = value;
    }
  }

  // Must return total_size to indicate all data was processed
  return total_size;
}

/**
 * @brief Gets the MIME type (Content-Type header) of a resource at a URL using
 * libcurl.
 *
 * @param url The URL of the resource (e.g., an image).
 * @return The MIME type string (e.g., "image/jpeg", "image/png") if found,
 *         or an empty string if the Content-Type header is not found or an
 * error occurs.
 */
std::string getMEMI(std::string const& url, std::string const& proxy) {
  CURL* curl = nullptr;
  CURLcode res = CURLE_OK;
  std::string content_type;  // This will store the result

  // Initialize CURL easy handle
  curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Error: curl_easy_init() failed" << std::endl;
    return "";  // Return empty on init failure
  }

  // Set the URL
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  // Perform a HEAD request (we only need headers, not the body)
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

  // Follow redirects (important for many URLs)
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // Set the header callback function to process headers
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

  // Pass the address of our content_type string to the callback
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &content_type);

  // Set a timeout (e.g., 10 seconds)
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);  // 30 秒总超时
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                   3000L);  // 3 秒连接超时

  if (!proxy.empty()) {
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
  }

  // For HTTPS: Verify peer and host (recommended for security)
  // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Default is 1L
  // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); // Default is 2L
  // If you have certificate issues in testing, you MIGHT temporarily disable
  // these but this is NOT recommended for production: curl_easy_setopt(curl,
  // CURLOPT_SSL_VERIFYPEER, 0L); curl_easy_setopt(curl,
  // CURLOPT_SSL_VERIFYHOST, 0L);

  // Perform the request
  res = curl_easy_perform(curl);

  // Check for errors during the request
  if (res != CURLE_OK) {
    std::cerr << "Error: curl_easy_perform() failed for URL '" << url
              << "': " << curl_easy_strerror(res) << std::endl;
    content_type.clear();  // Ensure empty string on error
  } else {
    // Check HTTP response code (optional but good)
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
      std::cerr << "Warning: HTTP error " << http_code << " for URL '" << url
                << "'" << std::endl;
      // Depending on requirements, you might want to clear content_type
      // here too content_type.clear();
    }
    // If content_type is still empty after a successful request,
    // it means the server didn't send a Content-Type header.
    if (content_type.empty() && http_code < 400) {
      std::cerr << "Warning: No Content-Type header found for URL '" << url
                << "'" << std::endl;
    }
  }

  // Cleanup CURL easy handle
  curl_easy_cleanup(curl);

  return content_type;
}

std::string timestamp(const char* format) {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
#if defined(_WIN32)
  struct tm tm;
  localtime_s(&tm, &time_t_now);
#else
  auto tm = *std::localtime(&time_t_now);
#endif
  char buf[128];
  auto const ret = std::strftime(buf, std::size(buf), format, &tm);
  if (ret == 0) {
    buf[0] = '\0';
  }
  return std::string(buf);
}

std::string app_data_dir(const std::string& app,
                         [[maybe_unused]] const std::string& author) {
#if defined(_WIN32)
  auto userprofile = env::get("USERPROFILE");
  if (userprofile) {
    return userprofile.value() + R"(\AppData\Local\)" +
           (author.empty() ? "Ai\\" : author + "\\") + app;
  }
  auto home_drive = env::get("HOMEDRIVE");
  auto home_path = env::get("HOMEPATH");
  if (home_drive.has_value() && home_path.has_value()) {
    return home_drive.value() + home_path.value() + R"(\AppData\Local\)" +
           (author.empty() ? "Ai\\" : author + "\\") + app;
  }
#elif defined(__APPLE__)
  auto home_dir = env::get("HOME");
  if (home_dir.has_value()) {
    return home_dir.value() + "/Library/Application Support/" + app;
  }
#else
  auto home_dir = env::get("HOME");
  if (home_dir.has_value()) {
    return home_dir.value() + "/.local/share/" + app;
  }
#endif
  return std::filesystem::current_path().string();
}

#if defined(_WIN32)
namespace {
// Helper function to convert a UTF-8 std::string to a UTF-16 std::wstring
inline std::wstring to_wstring(const std::string& utf8str,
                               const UINT from_codepage = CP_UTF8) {
  if (utf8str.empty()) {
    return {};
  }
  int size_needed = MultiByteToWideChar(from_codepage, 0, &utf8str[0],
                                        (int)utf8str.size(), NULL, 0);
  if (size_needed <= 0) {
    // Consider throwing an exception for conversion errors
    return {};
  }
  std::wstring utf16str(size_needed, 0);
  MultiByteToWideChar(from_codepage, 0, &utf8str[0], (int)utf8str.size(),
                      &utf16str[0], size_needed);
  return utf16str;
}

// Helper function to convert a UTF-16 std::wstring to a UTF-8 std::string
inline std::string to_string(const std::wstring& utf16str,
                             const UINT to_codepage = CP_UTF8) {
  if (utf16str.empty()) {
    return {};
  }
  int size_needed = WideCharToMultiByte(
      to_codepage, 0, &utf16str[0], (int)utf16str.size(), NULL, 0, NULL, NULL);
  if (size_needed <= 0) {
    // Consider throwing an exception for conversion errors
    return {};
  }
  std::string utf8str(size_needed, 0);
  WideCharToMultiByte(to_codepage, 0, &utf16str[0], (int)utf16str.size(),
                      &utf8str[0], size_needed, NULL, NULL);
  return utf8str;
}
}  // namespace
std::optional<std::string> toUtf8(const std::string& s) {
  auto u8 = to_string(to_wstring(s, GetACP()), CP_UTF8);
  if (!u8.empty()) {
    return u8;
  }
  return std::nullopt;
}
#endif

std::optional<std::string> read_file(std::string const& path) {
  // On some platforms (e.g. Linux/GCC) std::ifstream::open() can succeed on a
  // directory, but reading from it later throws an exception.  Check early.
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec)) {
    return std::nullopt;
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return std::nullopt;
  }
  return std::string{std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>()};
}

bool write_file(std::string const& path, std::string const& content) {
  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.flush();
  return file.good();
}

#if defined(_WIN32)
using NativeHandle = HANDLE;
#else   // _WIN32
using NativeHandle = int;
#endif  // !_WIN32
static bool is_atty(NativeHandle f) {
#if defined(_WIN32)
  return GetFileType(f) == FILE_TYPE_CHAR;
#else
  return isatty(f);
#endif
}

bool stdin_is_atty() {
#if defined(_WIN32)
  return is_atty(GetStdHandle(STD_INPUT_HANDLE));
#else
  return is_atty(STDIN_FILENO);
#endif
}

bool stdout_is_atty() {
#if defined(_WIN32)
  return is_atty(GetStdHandle(STD_OUTPUT_HANDLE));
#else
  return is_atty(STDOUT_FILENO);
#endif
}

bool stderr_is_atty() {
#if defined(_WIN32)
  return is_atty(GetStdHandle(STD_ERROR_HANDLE));
#else
  return is_atty(STDERR_FILENO);
#endif
}

}  // namespace ai::utils
