#pragma once

#include <argparse/argparse.hpp>
#include <optional>
#include <set>
#include <string>

#include "ai/logging.h"
#include "ai/utils.h"

namespace ai {

struct AiArgs {
  struct ChatArgs {
    std::vector<std::string> prompts;
    std::string model;
    std::string api_url;
    std::optional<std::string> system_prompt;
    std::optional<unsigned int> max_tokens;
    std::optional<unsigned int> n;
    std::optional<double> temperature;
    std::optional<double> top_p;
    std::optional<bool> thinking{std::nullopt};
    std::optional<std::string> reasoning_effort;
    bool stream{utils::stdout_is_atty()};
    bool stream_include_usage{false};
    std::set<std::string> tools;
    std::optional<std::string> tool_choice;
    bool continue_with_last_history{false};
    std::optional<std::string> continue_with_history_id;
    bool no_tools{false};
    bool list_tools{false};
    std::optional<std::string> topic_base_url;
    std::optional<std::string> topic_api_key;
    std::optional<std::string> topic_model;
  };

  struct ModelsArgs {
    std::string api_url;
  };

  struct HistoryArgs {
    /// Number of recent sessions to list; 0 means all.
    int n{1};
    /// Output format: "text" (human-readable) or "json".
    std::string format{"text"};
  };

  struct UpdateArgs {
    /// Force update even if already on the latest version.
    bool force{false};
  };

  bool help{false};
  bool version{false};
#if defined(NDEBUG)
  int log_level = ::ai::logging::LOGGING_FATAL;
#else
  int log_level = ::ai::logging::LOGGING_ERROR;
#endif
  std::optional<std::string> log_file;
  std::optional<std::string> proxy;
  std::string api_key;
  ChatArgs chat_args;
  ModelsArgs models_args;
  HistoryArgs history_args;
  UpdateArgs update_args;
  bool print_bash_completion{false};
  bool print_zsh_completion{false};
  bool print_fish_completion{false};
};

argparse::ArgParser get_parser(AiArgs& args);

}  // namespace ai
