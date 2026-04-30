#include "ai/system_prompt.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <environment/environment.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <subprocess/subprocess.hpp>
#include <vector>

namespace ai {

namespace {

/// Generate a simple indented tree view of a directory (limited depth).
/// Directories matching names in |skip_dirs| are displayed but not recursed.
std::string make_tree(std::filesystem::path const& root, int max_depth = 2,
                      int max_entries_per_dir = 50,
                      std::vector<std::string> const& skip_dirs = {}) {
  std::string out;
  auto build = [&](auto& self, std::filesystem::path const& dir,
                   std::string const& prefix, int depth) -> void {
    if (depth > max_depth) {
      return;
    }
    std::error_code ec;
    int count = 0;
    std::vector<std::filesystem::directory_entry> entries;
    for (auto const& e : std::filesystem::directory_iterator(dir, ec)) {
      entries.push_back(e);
      if (++count >= max_entries_per_dir) {
        break;
      }
    }
    // Sort: directories first, then files, alphabetically
    std::sort(entries.begin(), entries.end(), [](auto const& a, auto const& b) {
      if (a.is_directory() != b.is_directory()) {
        return a.is_directory() > b.is_directory();
      }
      return a.path().filename() < b.path().filename();
    });
    for (size_t i = 0; i < entries.size(); ++i) {
      bool last = (i == entries.size() - 1);
      std::string branch = last ? "\u2514\u2500\u2500 " : "\u251C\u2500\u2500 ";
      std::string next_prefix = prefix + (last ? "    " : "\u2502   ");
      auto const& entry = entries[i];
      auto name = entry.path().filename().string();
      if (entry.is_directory(ec)) {
        bool skip = std::find(skip_dirs.begin(), skip_dirs.end(), name) !=
                    skip_dirs.end();
        if (skip) {
          out += prefix + branch + name + "/ (skipped)\n";
        } else {
          out += prefix + branch + name + "/\n";
          self(self, entry.path(), next_prefix, depth + 1);
        }
      } else {
        out += prefix + branch + name + "\n";
      }
    }
  };
  out += root.string() + "\n";
  build(build, root, "", 0);
  return out;
}

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

  // ── 1. Current date & time ───────────────────────────────────────
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::string time_str = std::ctime(&now_time);
  if (!time_str.empty() && time_str.back() == '\n') {
    time_str.pop_back();
  }
  prompt += "Current time: " + time_str + "\n\n";

  // ── 2. Current working directory ─────────────────────────────────
  std::error_code ec;
  prompt += "Current working directory: " +
            std::filesystem::current_path(ec).string() + "\n\n";

  // ── 3. Operating system ──────────────────────────────────────────
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
  prompt += "Operating system: " + os_name + "\n\n";

  // ── 4. Shell ─────────────────────────────────────────────────────
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

  // ── 5. Git repository context ────────────────────────────────────
  auto git_root = find_git_root();
  if (git_root.has_value()) {
    prompt += "You are inside a git repository.\n";
    prompt += "Repository root: " + git_root.value() + "\n";

    // Current branch
    auto [ret_br, out_br, err_br] =
        subprocess::capture_run(std::vector<std::string>{
            "git", "-C", git_root.value(), "branch", "--show-current"});
    if (ret_br == 0) {
      std::string branch = out_br.to_string();
      if (!branch.empty() && branch.back() == '\n') {
        branch.pop_back();
      }
      prompt +=
          "Current branch: " + (branch.empty() ? "(detached HEAD)" : branch) +
          "\n";
    }
    // Git status (porcelain)
    auto [ret_st, out_st,
          err_st] = subprocess::capture_run(std::vector<std::string>{
        "git", "-C", git_root.value(), "status", "--porcelain", "--branch"});
    std::string status = out_st.to_string();
    if (!status.empty()) {
      prompt += "Git status:\n" + status + "\n";
    }

    // Repository structure (top 2 levels, skip .git and build dirs)
    prompt += "\nRepository structure:\n" +
              make_tree(std::filesystem::path(git_root.value()), 2, 60,
                        {".git", "build", "_deps", "third_party"});
  }

  return prompt;
}

}  // namespace ai
