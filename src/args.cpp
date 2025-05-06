#include "args.h"

#include <argparse.hpp>
#include <cstdlib>
#include <fstream>

#include "./utils.h"

namespace {
static void bind_model_args(argparse::ArgParser& parser, AiArgs& args) {
    auto& models = parser.add_command("models", "list models");

    auto& models_args = args.models_args;
    models.add_option("k,key", "OpenAI API key", models_args.api_key)
        .value_help("key");
    models.add_option("u,url", "OpenAI API Compatible URL", models_args.api_url)
        .default_value("https://api.deepseek.com/models");
    ;

    models.add_alias(
        "qwen", "url",
        "https://dashscope.aliyuncs.com/compatible-mode/v1/models");

    models.add_alias(
        "gemini", "url",
        "https://generativelanguage.googleapis.com/v1beta/openai/models");
    models.add_alias(
        "google", "url",
        "https://generativelanguage.googleapis.com/v1beta/openai/models");

    models.add_alias("deepseek", "url", "https://api.deepseek.com/models");

    models.add_alias("openai", "url", "https://api.openai.com/v1/models");

    models.callback([&args, &models]() -> void {
        if (args.help) {
            models.print_usage();
            exit(EXIT_SUCCESS);
        }
        if (args.models_args.api_key.empty() &&
            !args.models_args.api_url.empty()) {
            auto& url = args.models_args.api_url;
            if (url.find("api.deepseek.com/") != std::string::npos) {
                if (auto* env = std::getenv("DEEPSEEK_API_KEY");
                    env != nullptr) {
                    args.models_args.api_key = env;
                }
            } else if (url.find("api.openai.com/") != std::string::npos) {
                if (auto* env = std::getenv("OPENAI_API_KEY"); env != nullptr) {
                    args.models_args.api_key = env;
                }
            } else if (url.find("generativelanguage.googleapis.com/") !=
                       std::string::npos) {
                if (auto* env = std::getenv("GEMINI_API_KEY"); env != nullptr) {
                    args.models_args.api_key = env;
                }
            } else if (url.find("dashscope.aliyuncs.com/") !=
                       std::string::npos) {
                if (auto* env = std::getenv("QWEN_API_KEY"); env != nullptr) {
                    args.models_args.api_key = env;
                }
            }
        }

        if (!args.proxy.has_value()) {
            auto& url = args.models_args.api_url;
            if (url.find("api.openai.com/") != std::string::npos) {
                if (auto* env = std::getenv("OPENAI_API_PROXY");
                    env != nullptr) {
                    args.proxy = env;
                }
            } else if (url.find("generativelanguage.googleapis.com/") !=
                       std::string::npos) {
                if (auto* env = std::getenv("GEMINI_API_PROXY");
                    env != nullptr) {
                    args.proxy = env;
                }
            }
        }
    });
}
static void bind_chat_args(argparse::ArgParser& parser, AiArgs& args) {
    auto& chat = parser.add_command("chat", "ai chatbot");
    auto& chat_args = args.chat_args;
    chat.add_flag("i,interactive", "Enable interactive mode",
                  chat_args.interactive);
    chat.add_negative_flag("I", "Disable interactive mode",
                           chat_args.interactive);
    chat.add_flag("stream", "Enable streaming mode", chat_args.stream)
        .negatable();
    chat.add_flag("v,verbose", "Enable verbose mode", chat_args.verbose);
    chat.add_flag("version", "Show version", chat_args.version);

    chat.add_option("k,key", "OpenAI API key", chat_args.api_key)
        .value_help("key");
    chat.add_option("m,model", "Model to use", chat_args.model)
        .default_value("deepseek-chat");
    chat.add_option("p,prompt", "Prompt", chat_args.prompt);
    chat.add_option("s,system-prompt", "System prompt",
                    chat_args.system_prompt);
    chat.add_option("t,temperature", "Model temperature",
                    chat_args.temperature);
    chat.add_option("top-p", "Model top-p parameter", chat_args.top_p);
    chat.add_option("u,url", "OpenAI API Compatible URL", chat_args.api_url)
        .default_value("https://api.deepseek.com/chat/completions");
    chat.add_alias("qwen", "url",
                   "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/"
                   "completions");

    chat.add_alias("gemini", "url",
                   "https://generativelanguage.googleapis.com/v1beta/openai/"
                   "chat/completions");
    chat.add_alias("google", "url",
                   "https://generativelanguage.googleapis.com/v1beta/openai/"
                   "chat/completions");

    chat.add_alias("deepseek", "url",
                   "https://api.deepseek.com/chat/completions");

    chat.add_alias("openai", "url",
                   "https://api.openai.com/v1/chat/completions");

    chat.add_positional("prompts", "Prompt", chat_args.prompt);

    chat.callback([&args, &chat]() -> void {
        if (args.help) {
            chat.print_usage();
            exit(EXIT_SUCCESS);
        }

        if (args.chat_args.model.empty()) {
            args.chat_args.model = [](std::string const& url) {
                if (url ==
                    "https://dashscope.aliyuncs.com/compatible-mode/v1") {
                } else if (url ==
                           "https://generativelanguage.googleapis.com/"
                           "v1beta/openai") {
                    return "gemini-2.0-flash";
                } else if (url == "https://api.deepseek.com/chat/completions") {
                    return "deepseek-chat";  // deepseek-reasoner
                }
                return "";
            }(args.chat_args.api_url);
        }

        if (args.chat_args.model.empty()) {
            std::cerr << "Error: Must provide an ai model." << std::endl;
            exit(EXIT_FAILURE);
        }

        if (!args.chat_args.interactive && args.chat_args.prompt.empty()) {
            try {
                args.chat_args.prompt = getUserInputViaEditor();
            } catch (...) {
            }
            if (args.chat_args.prompt.empty()) {
                std::cerr
                    << "Error: Must provide a prompt or use interactive mode."
                    << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        if (args.chat_args.api_key.empty() && !args.chat_args.api_url.empty()) {
            auto& url = args.chat_args.api_url;
            if (url.find("api.deepseek.com/") != std::string::npos) {
                if (auto* env = std::getenv("DEEPSEEK_API_KEY");
                    env != nullptr) {
                    args.chat_args.api_key = env;
                }
            } else if (url.find("api.openai.com/") != std::string::npos) {
                if (auto* env = std::getenv("OPENAI_API_KEY"); env != nullptr) {
                    args.chat_args.api_key = env;
                }
            } else if (url.find("generativelanguage.googleapis.com/") !=
                       std::string::npos) {
                if (auto* env = std::getenv("GEMINI_API_KEY"); env != nullptr) {
                    args.chat_args.api_key = env;
                }
            } else if (url.find("dashscope.aliyuncs.com/") !=
                       std::string::npos) {
                if (auto* env = std::getenv("QWEN_API_KEY"); env != nullptr) {
                    args.chat_args.api_key = env;
                }
            }
        }

        if (!args.proxy.has_value()) {
            auto& url = args.chat_args.api_url;
            if (url.find("api.openai.com/") != std::string::npos) {
                if (auto* env = std::getenv("OPENAI_API_PROXY");
                    env != nullptr) {
                    args.proxy = env;
                }
            } else if (url.find("generativelanguage.googleapis.com/") !=
                       std::string::npos) {
                if (auto* env = std::getenv("GEMINI_API_PROXY");
                    env != nullptr) {
                    args.proxy = env;
                }
            }
        }

        if (args.chat_args.prompt == "-") {
            args.chat_args.prompt =
                std::string{std::istreambuf_iterator<char>(std::cin),
                            std::istreambuf_iterator<char>()};
        } else if (args.chat_args.prompt.starts_with("@")) {
            std::string file_name = args.chat_args.prompt.substr(1);
            std::ifstream file(file_name);
            if (!file) {
                std::cerr << "Error: Cannot open file: " << file_name
                          << std::endl;
                exit(EXIT_FAILURE);
            }
            args.chat_args.prompt =
                std::string{std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>()};
        }
    });
}

}  // namespace

AiArgs& AiArgs::instance() {
    static AiArgs args;
    return args;
}

argparse::Command& AiArgs::parse(int argc, char* argv[]) {
    try {
        auto& cmd = parser.parse(argc, (const char**)argv);
        return cmd;
    } catch (std::exception const& e) {
        std::cerr << e.what() << "\n";
        exit(EXIT_FAILURE);
    }
}

AiArgs::AiArgs() : parser("ai", "OpenAI API Compatible Command Line Chatbot") {
    parser.add_flag("h,help", "show this help info", help);
    parser.add_flag("d,debug", "Enable debug mode", debug).negatable();
    parser.add_option("proxy", "Use proxy", proxy);
    parser.callback([this]() {
        if (help) {
            parser.print_usage();
            exit(EXIT_SUCCESS);
        }
    });
    bind_chat_args(parser, *this);
    bind_model_args(parser, *this);
}
