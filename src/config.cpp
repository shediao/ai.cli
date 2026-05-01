#include "ai/config.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "ai/utils.h"
#include "nlohmann/json.hpp"

namespace ai {

const ProviderConfig* AppConfig::find_provider(const std::string& alias) const {
  for (auto& p : providers) {
    if (p.alias == alias) {
      return &p;
    }
  }
  return nullptr;
}

std::string config_file_path() {
  auto dir = app_data_dir("ai.cli");
  return dir + "/config.json";
}

static AppConfig const& default_config() {
  static AppConfig config;
  config.providers = {
      {"deepseek", "https://api.deepseek.com", std::nullopt, "deepseek-v4-pro"},
      {"openai", "https://api.openai.com/v1", std::nullopt, "gpt-4o"},
      {"gemini", "https://generativelanguage.googleapis.com/v1beta/openai",
       std::nullopt, "gemini-flash-latest"},
      {"qwen", "https://dashscope.aliyuncs.com/compatible-mode/v1",
       std::nullopt, "qwen-max-latest"},
      {"moonshot", "https://api.moonshot.cn/v1", std::nullopt, "kimi-k2.6"},
      {"ollama", "http://127.0.0.1:11434/v1", std::nullopt, std::nullopt},
  };
  return config;
}

void write_default_config_if_not_exists(const std::string& path) {
  if (std::filesystem::exists(path)) {
    return;
  }

  auto dir = std::filesystem::path(path).parent_path();
  if (!dir.empty() && !std::filesystem::exists(dir)) {
    std::filesystem::create_directories(dir);
  }

  nlohmann::json j;
  nlohmann::json providers = nlohmann::json::array();
  for (auto& p : default_config().providers) {
    nlohmann::json provider;
    provider["alias"] = p.alias;
    provider["base_url"] = p.base_url;
    if (p.api_key.has_value()) {
      provider["api_key"] = p.api_key.value();
    }
    if (p.default_model.has_value()) {
      provider["default_model"] = p.default_model.value();
    }
    providers.push_back(provider);
  }
  j["providers"] = providers;
  j["version"] = default_config().version;

  std::ofstream out(path);
  if (out.is_open()) {
    out << j.dump(2) << '\n';
    std::cerr << "Config file created at: " << path
              << "\nPlease edit it to set your API keys.\n";
  }
}

AppConfig load_config() {
  auto path = config_file_path();

  // Write default config if not exists
  write_default_config_if_not_exists(path);

  AppConfig config;

  try {
    std::ifstream in(path);
    if (!in.is_open()) {
      std::cerr << "Warning: Could not open config file: " << path << "\n";
      return default_config();
    }

    nlohmann::json j;
    in >> j;

    if (j.contains("providers") && j["providers"].is_array()) {
      for (auto& pj : j["providers"]) {
        ProviderConfig pc;
        pc.alias = pj.value("alias", "");
        pc.base_url = pj.value("base_url", "");

        if (pj.contains("api_key") && pj["api_key"].is_string() &&
            !pj["api_key"].get<std::string>().empty()) {
          pc.api_key = pj["api_key"].get<std::string>();
        }
        if (pj.contains("default_model") && pj["default_model"].is_string() &&
            !pj["default_model"].get<std::string>().empty()) {
          pc.default_model = pj["default_model"].get<std::string>();
        }

        if (!pc.alias.empty() && !pc.base_url.empty()) {
          config.providers.push_back(std::move(pc));
        }
      }
    }

    if (j.contains("version") && j["version"].is_number()) {
      config.version = j["version"].get<int>();
      if (config.version > default_config().version) {
        std::cerr << "Warning: Config file is from a newer version.\n";
      }
    } else {
      config.version = 0;
    }

    if (config.providers.empty()) {
      std::cerr << "Warning: No valid providers found in config, using "
                   "defaults.\n";
      return default_config();
    }
  } catch (const nlohmann::json::exception& e) {
    std::cerr << "Warning: Failed to parse config file: " << e.what() << "\n";
    return default_config();
  }

  return config;
}

}  // namespace ai
