#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ai {

struct ProviderConfig {
  std::string alias;
  std::string base_url;
  std::optional<std::string> api_key;
  std::optional<std::string> default_model;
};

struct AppConfig {
  std::vector<ProviderConfig> providers;

  // Look up a provider by alias
  const ProviderConfig* find_provider(const std::string& alias) const;
};

// Load config from the OS-standard config path.
// If the file doesn't exist, a default config is written and returned.
AppConfig load_config();

// Get the config file path (OS-standard application config directory)
std::string config_file_path();

// Write a default config file to the config path (if it doesn't exist)
void write_default_config_if_not_exists(const std::string& path);

}  // namespace ai
