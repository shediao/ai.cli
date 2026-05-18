#pragma once

#include <string>

namespace ai {

/// Build a default system prompt with useful context information
/// (cwd, OS, shell, git repository context).
std::string build_default_system_prompt();

}  // namespace ai
