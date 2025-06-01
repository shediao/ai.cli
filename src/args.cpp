#include "args.h"

#include <algorithm>
#include <argparse/argparse.hpp>
#include <cstdlib>
#include <fstream>

#if defined(_WIN32) || defined(_Win64)
#include <io.h>
#include <stdio.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include "./utils.h"

namespace {

static const std::string gemini_base_url =
    "https://generativelanguage.googleapis.com/v1beta/openai/";
static const std::string qwen_base_url =
    "https://dashscope.aliyuncs.com/compatible-mode/v1/";
static const std::string deepseek_base_url = "https://api.deepseek.com/";
static const std::string openai_base_url = "https://api.openai.com/v1/";
static const std::string moonshot_base_url = "https://api.moonshot.cn/v1";
static const std::string ollama_base_url = "http://127.0.0.1:11434/v1";

static std::optional<std::string> getProxyFromEnvironment(
    std::string const& url) {
  static std::map<std::string, std::string> url_proxy{
      {openai_base_url, "OPENAI_API_PROXY"},
      {gemini_base_url, "GEMINI_API_PROXY"}};
  auto it = std::find_if(url_proxy.begin(), url_proxy.end(),
                         [&url](auto const& entry) {
                           return url.find(entry.first) != std::string::npos;
                         });
  if (it != url_proxy.end()) {
    if (auto* env = std::getenv(it->second.c_str())) {
      return env;
    }
  }
  return std::nullopt;
}

static std::optional<std::string> getApiKeyFromEnvironment(
    std::string const& url) {
  static std::map<std::string, std::string> url_key{
      {deepseek_base_url, "DEEPSEEK_API_KEY"},
      {openai_base_url, "OPENAI_API_KEY"},
      {gemini_base_url, "GEMINI_API_KEY"},
      {qwen_base_url, "QWEN_API_KEY"},
      {moonshot_base_url, "MOONSHOT_API_KEY"}};
  auto it =
      std::find_if(url_key.begin(), url_key.end(), [&url](auto const& entry) {
        return url.find(entry.first) != std::string::npos;
      });
  if (it != url_key.end()) {
    if (auto* env = std::getenv(it->second.c_str())) {
      return env;
    }
  }
  return std::nullopt;
}

std::optional<std::string> getDefaultModelForUrl(std::string const& url) {
  if (url == qwen_base_url + "chat/completions") {
    return "qwen-turbo-latest";  // qwen-plus,qwen-turbo
  } else if (url == gemini_base_url + "chat/completions") {
    return "gemini-2.0-flash";
  } else if (url == deepseek_base_url + "chat/completions") {
    return "deepseek-chat";  // deepseek-reasoner
  } else if (url == moonshot_base_url + "chat/completions") {
    return "moonshot-v1-auto";
  }
  return std::nullopt;
}

static void add_alias_options(argparse::Command& command) {
  command.add_alias("qwen", "base-url", qwen_base_url);
  command.add_alias("gemini", "base-url", gemini_base_url);
  command.add_alias("google", "base-url", gemini_base_url);
  command.add_alias("deepseek", "base-url", deepseek_base_url);
  command.add_alias("openai", "base-url", openai_base_url);
  command.add_alias("moonshot", "base-url", moonshot_base_url);
  command.add_alias("ollama", "base-url", ollama_base_url);
}

static void bind_model_args(argparse::ArgParser& parser, AiArgs& args) {
  auto& models = parser.add_command("models", "list models");

  auto& models_args = args.models_args;
  models.add_option("u,url", "OpenAI API Compatible URL", models_args.api_url)
      .hidden();
  models
      .add_option("base-url", "OpenAI API Compatible URL(<base_url>/models)",
                  models_args.api_url)
      .default_value(deepseek_base_url)
      .callback([&models_args](std::string const& base_url) {
        if (!base_url.empty()) {
          models_args.api_url =
              base_url +
              (base_url[base_url.size() - 1] == '/' ? "models" : "/models");
        }
      });
  add_alias_options(models);

  models.callback([&args]() -> void {
    auto& models_args = args.models_args;
    if (args.api_key.empty() && !models_args.api_url.empty()) {
      auto key = getApiKeyFromEnvironment(models_args.api_url);
      if (key.has_value()) {
        args.api_key = key.value();
      }
    }

    if (!args.proxy.has_value()) {
      auto proxy = getProxyFromEnvironment(models_args.api_url);
      if (proxy.has_value()) {
        args.proxy = proxy.value();
      }
    }
  });
}

bool stdin_is_atty() {
#if defined(_WIN32) || defined(_Win64)
  return _isatty(_fileno(stdin));
#elif defined(__linux__) || defined(__APPLE__)
  return isatty(STDIN_FILENO);
#endif
  return false;
}

static void bind_chat_args(argparse::ArgParser& parser, AiArgs& args) {
  auto& chat = parser.add_command("chat", "ai chatbot");
  auto& chat_args = args.chat_args;
  chat.add_flag("stream", "Enable streaming mode", chat_args.stream)
      .negatable();
  chat.add_flag("stream-include-usage", "print usage in streaming mode",
                chat_args.stream_include_usage)
      .negatable();

  chat.add_flag("C", "continue with last history",
                chat_args.continue_with_last_history);

  chat.add_option("m,model", "Model to use", chat_args.model);
  chat.add_option("s,system-prompt", "System prompt", chat_args.system_prompt);
  chat.add_option("t,temperature", "Model temperature", chat_args.temperature);
  chat.add_option("top-p", "Model top-p parameter", chat_args.top_p);
  chat.add_option("u,url", "OpenAI API Compatible URL", chat_args.api_url)
      .hidden();
  chat.add_option("base-url",
                  "OpenAI API Compatible URL(<base_url>/chat/completions)",
                  chat_args.api_url)
      .default_value(deepseek_base_url)
      .callback([&args](std::string const& base_url) {
        if (!base_url.empty()) {
          args.chat_args.api_url =
              base_url + (base_url[base_url.size() - 1] == '/'
                              ? "chat/completions"
                              : "/chat/completions");
        }
      });

  chat.add_option("max-tokens", "max tokens", chat_args.max_tokens);
  chat.add_option("reasoning-effort", "reasoning effort",
                  chat_args.reasoning_effort)
      .choices({"low", "medium", "high", "none"});

  chat.add_option("tools", "tools call", chat_args.tools)
      .allowed({"filesystem"});

  add_alias_options(chat);

  chat.add_positional("prompts", "user prompts", chat_args.prompts);

  chat.callback([&args]() -> void {
    auto& chat_args = args.chat_args;

    if (chat_args.model.empty()) {
      auto model = getDefaultModelForUrl(chat_args.api_url);
      if (model.has_value()) {
        chat_args.model = model.value();
      }
    }

    if (chat_args.model.empty()) {
      std::cerr << "Error: Must provide an ai model." << std::endl;
      exit(EXIT_FAILURE);
    }

    // read from stdin
    if (auto it =
            std::find(begin(chat_args.prompts), end(chat_args.prompts), "-");
        it != end(chat_args.prompts)) {
      *it = std::string(std::istreambuf_iterator<char>(std::cin),
                        std::istreambuf_iterator<char>{});
    };

    if (chat_args.prompts.empty() && stdin_is_atty()) {
      try {
        if (auto prompt = getUserInputViaEditor(); !prompt.empty()) {
          chat_args.prompts.push_back(prompt);
        }
      } catch (...) {
      }
    }
    if (chat_args.prompts.empty()) {
      std::cerr << "Error: Must provide a prompt." << std::endl;
      exit(EXIT_FAILURE);
    }

    if (args.api_key.empty() && !chat_args.api_url.empty()) {
      auto key = getApiKeyFromEnvironment(chat_args.api_url);
      if (key.has_value()) {
        args.api_key = key.value();
      }
    }

    if (!args.proxy.has_value()) {
      if (!args.proxy.has_value()) {
        auto proxy = getProxyFromEnvironment(chat_args.api_url);
        if (proxy.has_value()) {
          args.proxy = proxy.value();
        }
      }
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
  parser.add_option("x,proxy", "Use proxy(curl)", proxy).value_help("PROXY");
  parser.add_option("k,key", "API key", api_key).value_help("key");
  parser
      .add_option("log-level", "set logging level", log_level)
#if defined(NDEBUG)
      .default_value("4")
#else
      .default_value("3")
#endif
      .allowed({"0", "1", "2", "3", "5"})
      .allowed_help({{"0", "DEBUG"},
                     {"1", "INFO"},
                     {"2", "WARNING"},
                     {"3", "ERROR"},
                     {"4", "FATAL"}});
  parser.add_negative_flag("v", "decrement log level", log_level);
  parser.add_option("enable-logging", "log output to stderr/file", log_type)
      .default_value("stderr")
      .allowed({"file", "stderr", "all"});
  parser.add_option("log-file", "log file path", log_file)
      .default_value("debug.log");
  bind_chat_args(parser, *this);
  bind_model_args(parser, *this);
}
