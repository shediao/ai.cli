#ifndef __AI_CLI_ARGS_H__
#define __AI_CLI_ARGS_H__
#include <argparse/argparse.hpp>
#include <optional>
#include <set>
#include <string>

struct AiArgs {
  struct ChatArgs {
    std::vector<std::string> prompts;
    std::string model;
    std::string api_url;
    std::optional<std::string> system_prompt;
    std::optional<int> max_tokens;
    std::optional<double> temperature;
    std::optional<double> top_p;
    std::optional<std::string> reasoning_effort;
    bool stream{false};
    bool stream_include_usage{false};
    std::set<std::string> tools;
    bool continue_with_last_history{false};
  };

  struct ModelsArgs {
    std::string api_url;
  };

  bool help{false};
  bool debug{false};
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
