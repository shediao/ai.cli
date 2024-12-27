#include "cli_parser.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    std::string content{std::istreambuf_iterator<char>(file),
                        std::istreambuf_iterator<char>()};
    return content;
}
std::string read_stdin() {
    std::string content{std::istreambuf_iterator<char>(std::cin),
                        std::istreambuf_iterator<char>()};
    return content;
}

CLIParser::CLIParser()
    : parser("aichat", "OpenAI API Compatible Command Line Chatbot") {
    parser.add_flag("d,debug", "Enable debug mode", args.debug).negatable();
    parser.add_flag("h,help", "Show help", args.help);
    parser.add_flag("i,interactive", "Enable interactive mode",
                    args.interactive);
    parser.add_negative_flag("I", "Disable interactive mode", args.interactive);
    parser.add_flag("stream", "Enable streaming mode", args.stream).negatable();
    parser.add_flag("v,verbose", "Enable verbose mode", args.verbose);
    parser.add_flag("version", "Show version", args.version);

    parser.add_option("k,key", "OpenAI API key", args.api_key)
        .value_help("key");
    parser.add_option("m,model", "Model to use", args.model)
        .default_value("gpt-3.5-turbo");
    parser.add_option("p,prompt", "Prompt", args.prompt);
    parser.add_option("proxy", "Use proxy", args.proxy);
    parser.add_option("s,system-prompt", "System prompt", args.system_prompt);
    parser.add_option("t,temperature", "Model temperature", args.temperature);
    parser.add_option("top-p", "Model top-p parameter", args.top_p);
    parser.add_option("u,url", "OpenAI API Compatible URL", args.api_url)
        .default_value("https://api.openai.com/v1/chat/completions");

    parser.add_positional("prompts", "Prompt", args.prompt);
}

CLIParser::ParsedArgs& CLIParser::parse(int argc, const char* argv[]) {
    parser.parse(argc, argv);
    return args;
}

void CLIParser::print_help() { parser.print_usage(); }

void CLIParser::print_version() {
    std::cout << "openai-cli version 0.1.0" << std::endl;
}

OpenAIClient::Config CLIParser::to_client_config(const ParsedArgs& args) {
    OpenAIClient::Config config;

    if (args.api_key) {
        config.api_key = *args.api_key;
    }

    if (args.api_url) {
        config.api_url = *args.api_url;
    }

    if (args.model) {
        config.model = *args.model;
    }

    if (args.temperature) {
        config.temperature = *args.temperature;
    }

    if (args.top_p) {
        config.top_p = *args.top_p;
    }

    if (args.proxy) {
        config.proxy = *args.proxy;
    }

    config.stream = args.stream;
    config.debug = args.debug;
    config.verbose = args.verbose;

    return config;
}
