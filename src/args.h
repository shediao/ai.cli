#ifndef __AI_CLI_ARGS_H__
#define __AI_CLI_ARGS_H__
#include <argparse.hpp>
#include <optional>
#include <string>

struct AiArgs {
    struct ChatArgs {
        std::string prompt;
        std::string model;
        std::string api_key;
        std::string api_url;
        std::optional<std::string> system_prompt;
        std::optional<double> temperature;
        std::optional<double> top_p;
        bool stream{false};
        bool interactive{false};
        bool verbose{false};
        bool version{false};
    };

    struct ModelsArgs {
        std::string api_key;
        std::string api_url;
    };

    bool help{false};
    bool debug{false};
    std::optional<std::string> proxy;
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
