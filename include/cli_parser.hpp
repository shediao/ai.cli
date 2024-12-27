#pragma once

#include <argparse.hpp>
#include <optional>
#include <string>

#include "openai_client.hpp"

class CLIParser {
   public:
    CLIParser();
    ~CLIParser() = default;

    struct ParsedArgs {
        std::string prompt;
        std::optional<std::string> system_prompt;
        std::optional<std::string> model;
        std::optional<std::string> api_key;
        std::optional<std::string> api_url;
        std::optional<std::string> proxy;
        std::optional<double> temperature;
        std::optional<double> top_p;
        bool stream = false;
        bool interactive = false;
        bool debug = false;
        bool verbose = false;
        bool help = false;
        bool version = false;
    };

    ParsedArgs& parse(int argc, const char* argv[]);
    void print_help();
    void print_version();
    OpenAIClient::Config to_client_config(const ParsedArgs& args);

   private:
    argparse::ArgParser parser;
    ParsedArgs args{};
};
