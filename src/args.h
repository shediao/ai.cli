#ifndef __AI_CLI_ARGS_H__
#define __AI_CLI_ARGS_H__
#include <argparse/argparse.hpp>
#include <optional>
#include <set>
#include <string>

#include "logging.h"

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
    std::optional<std::string> reasoning_effort;
    bool stream{false};
    bool stream_include_usage{false};
    std::set<std::string> tools;
    std::optional<std::string> tool_choice;
    bool continue_with_last_history{false};
  };

  struct ModelsArgs {
    std::string api_url;
  };

  bool help{false};
#if defined(NDEBUG)
  int log_level = ::ai::logging::LOGGING_FATAL;
#else
  int log_level = ::ai::logging::LOGGING_ERROR;
#endif
  std::string log_type{"stderr"};
  std::string log_file;
  std::optional<std::string> proxy;
  std::string api_key;
  ChatArgs chat_args;
  ModelsArgs models_args;

  argparse::Command& parse(int argc, char* argv[]);
  static AiArgs& instance();
  AiArgs& operator=(AiArgs const&) = delete;
  AiArgs(AiArgs const&) = delete;
  AiArgs& operator=(AiArgs&&) = delete;
  AiArgs(AiArgs&&) = delete;
  ~AiArgs() = default;

 private:
  AiArgs();
  argparse::ArgParser parser;
};

#endif  // __AI_CLI_ARGS_H__
