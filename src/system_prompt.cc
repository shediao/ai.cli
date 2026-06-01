#include "ai/system_prompt.h"

#include <environment/environment.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <subprocess/subprocess.hpp>

namespace ai {

namespace {

/// Check if a path is inside a git repository by walking up to find .git.
std::optional<std::string> find_git_root() {
  std::error_code ec;
  auto current = std::filesystem::current_path(ec);
  for (auto p = current;; p = p.parent_path()) {
    if (std::filesystem::exists(p / ".git", ec)) {
      return p.string();
    }
    // has_parent_path() is unreliable: parent_path() of "/" can return "/" on
    // some implementations, causing an infinite loop. Compare instead.
    auto parent = p.parent_path();
    if (p == parent) {
      break;  // reached the filesystem root
    }
  }
  return std::nullopt;
}

}  // namespace

std::string build_default_system_prompt() {
  std::string prompt;

  // ── 1. Current working directory ─────────────────────────────────
  std::error_code ec;
  prompt += "Current working directory: " +
            std::filesystem::current_path(ec).string() + "\n\n";

  // ── 2. Operating system ──────────────────────────────────────────
  std::string os_name;
#if defined(_WIN32) || defined(_WIN64)
  os_name = "Windows";
  if (auto msys = env::get("MSYSTEM"); msys.has_value()) {
    os_name += " (MSYS2/" + msys.value() + ")";
  }
#elif defined(__APPLE__)
  os_name = "macOS (Darwin)";
#elif defined(__linux__)
  os_name = "Linux";
#elif defined(__FreeBSD__)
  os_name = "FreeBSD";
#elif defined(__OpenBSD__)
  os_name = "OpenBSD";
#else
  os_name = "Unix/POSIX";
#endif

  // Architecture detection
  std::string arch;
#if defined(__x86_64__) || defined(__amd64__) || defined(_M_AMD64)
  arch = "x86_64";
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
  arch = "arm64";
#elif defined(__i386__) || defined(__i686__) || defined(_M_IX86)
  arch = "x86";
#elif defined(__arm__) || defined(_M_ARM)
  arch = "arm";
#elif defined(__riscv) && (__riscv_xlen == 64)
  arch = "riscv64";
#else
  arch = "unknown";
#endif

  prompt += "Operating system: " + os_name + " (" + arch + ")\n\n";

  // ── 3. Shell ─────────────────────────────────────────────────────
  std::string shell = "unknown";
#if defined(_WIN32) || defined(_WIN64)
  if (auto comspec = env::get("COMSPEC"); comspec.has_value()) {
    shell = comspec.value();
  }
  if (auto ps = env::get("PSModulePath");
      ps.has_value() && ps.value()[0] != '\0') {
    shell = "PowerShell";
  }

  if (auto sh = env::get("SHELL"); sh.has_value()) {
    // bash, zsh, fish, dash, sh
    if (auto& s = sh.value(); s.ends_with("sh.exe") || s.ends_with("sh")) {
      shell = sh.value();
    }
  }
#else
  if (auto sh = env::get("SHELL"); sh.has_value()) {
    shell = sh.value();
  }
#endif
  prompt += "Default shell: " + shell + "\n\n";

  // ── 4. Git repository context ────────────────────────────────────
  auto git_root = find_git_root();
  if (git_root.has_value()) {
    prompt += "You are inside a git repository.\n";
    prompt += "Repository root: " + git_root.value() + "\n";

    // Current branch
    auto [ret_br, out_br, err_br] = subprocess::capture_run(
        "git", std::vector<std::string>{"-C", git_root.value(), "branch",
                                        "--show-current"});
    if (ret_br == 0) {
      std::string branch = out_br.to_string();
      if (!branch.empty() && branch.back() == '\n') {
        branch.pop_back();
      }
      prompt +=
          "Current branch: " + (branch.empty() ? "(detached HEAD)" : branch) +
          "\n";
    }
  }

  return prompt;
}

}  // namespace ai
