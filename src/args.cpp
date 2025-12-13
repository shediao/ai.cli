#include "args.h"

#include <algorithm>
#include <argparse/argparse.hpp>
#include <cstdlib>
#include <environment/environment.hpp>
#include <fstream>

#if defined(_WIN32) || defined(_Win64)
#include <io.h>
#include <stdio.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "utils.h"

namespace {

static const std::string gemini_base_url =
    "https://generativelanguage.googleapis.com/v1beta/openai";
static const std::string qwen_base_url =
    "https://dashscope.aliyuncs.com/compatible-mode/v1";
static const std::string deepseek_base_url = "https://api.deepseek.com";
static const std::string openai_base_url = "https://api.openai.com/v1";
static const std::string moonshot_base_url = "https://api.moonshot.cn/v1";
static const std::string ollama_base_url = "http://127.0.0.1:11434/v1";
static std::map<std::string, std::string> url_env_prefix{
    {deepseek_base_url, "DEEPSEEK"}, {openai_base_url, "OPENAI"},
    {gemini_base_url, "GEMINI"},     {qwen_base_url, "QWEN"},
    {moonshot_base_url, "MOONSHOT"}, {ollama_base_url, "OLLAMA"}};

static std::optional<std::string> getEnvironmentConfig(
    std::string const& url, std::string const& config_postfix) {
  auto it = std::find_if(url_env_prefix.begin(), url_env_prefix.end(),
                         [&url](auto const& entry) {
                           return url.find(entry.first) != std::string::npos;
                         });
  if (it != url_env_prefix.end()) {
    return env::get(it->second + config_postfix);
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
      auto key = getEnvironmentConfig(models_args.api_url, "_API_KEY");
      if (key.has_value()) {
        args.api_key = key.value();
      }
    }

    if (!args.proxy.has_value()) {
      auto proxy = getEnvironmentConfig(models_args.api_url, "_API_PROXY");
      if (proxy.has_value()) {
        args.proxy = proxy.value();
      }
    }
  });
}

inline static bool stdin_is_atty() {
#if defined(_WIN32) || defined(_WIN64)
  return _isatty(_fileno(stdin));
#else
  return isatty(STDIN_FILENO);
#endif
}

inline static bool stdin_is_pipe() {
#if defined(_WIN32) || defined(_WIN64)
  if (FILE_TYPE_PIPE == GetFileType(GetStdHandle(STD_INPUT_HANDLE))) {
    return true;
  }
  return false;
#else
  struct stat sb;
  if (0 == fstat(STDIN_FILENO, &sb)) {
    return (S_ISFIFO(sb.st_mode));
  }
  return false;
#endif
}

inline static bool stdin_is_file() {
#if defined(_WIN32) || defined(_WIN64)
  if (FILE_TYPE_DISK == GetFileType(GetStdHandle(STD_INPUT_HANDLE))) {
    return true;
  }
  return false;
#else
  struct stat sb;
  if (0 == fstat(STDIN_FILENO, &sb)) {
    return S_ISREG(sb.st_mode);
  }
  return false;
#endif
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

  chat.add_option("m,model", "Model to use", chat_args.model)
      .checker([](const std::string& model) {
        return std::pair<bool, std::string>{!model.empty(),
                                            "model is non empty string"};
      });
  chat.add_option("s,system-prompt", "System prompt", chat_args.system_prompt);
  chat.add_option("t,temperature", "Model temperature[0.0~2.0",
                  chat_args.temperature)
      .range(0.0, 2.0);
  chat.add_option("top-p", "Model top-p parameter [0.0~1.0]", chat_args.top_p)
      .range(0.0, 1.0);
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
      .choices({"filesystem"});
  chat.add_option("tool-choice",
                  "tool choice(none: if no tools, auto: if has tools)",
                  chat_args.tool_choice)
      .choices({"none", "auto", "required"});

  add_alias_options(chat);

  chat.add_positional("prompts", "user prompts", chat_args.prompts);

  chat.callback([&args]() -> void {
    auto& chat_args = args.chat_args;

    if (chat_args.model.empty()) {
      auto model = getEnvironmentConfig(chat_args.api_url, "_API_MODEL");
      if (!model) {
        model = getDefaultModelForUrl(chat_args.api_url);
      }
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
          std::cout << prompt;
          chat_args.prompts.push_back(prompt);
        }
      } catch (...) {
      }
    }
    if (stdin_is_pipe() || stdin_is_file()) {
      std::string read_content{std::istreambuf_iterator<char>(std::cin),
                               std::istreambuf_iterator<char>{}};
      if (!read_content.empty()) {
        std::cout << read_content;
        chat_args.prompts.push_back(std::move(read_content));
      }
    }
    if (chat_args.prompts.empty()) {
      std::cerr << "Error: Must provide a prompt." << std::endl;
      exit(EXIT_FAILURE);
    }

    if (args.api_key.empty() && !chat_args.api_url.empty()) {
      auto key = getEnvironmentConfig(chat_args.api_url, "_API_KEY");
      if (key.has_value()) {
        args.api_key = key.value();
      }
    }

    if (!args.proxy.has_value()) {
      auto proxy = getEnvironmentConfig(chat_args.api_url, "_API_PROXY");
      if (proxy.has_value()) {
        args.proxy = proxy.value();
      }
    }
  });
}

}  // namespace

AiArgs& AiArgs::instance() {
  static AiArgs args;
  return args;
}

#if defined(_WIN32)
argparse::Command& AiArgs::parse(int argc, wchar_t* argv[]) {
  try {
    auto& cmd = parser.parse(argc, (const wchar_t**)argv);
    return cmd;
  } catch (std::exception const& e) {
    std::cerr << e.what() << "\n";
    exit(EXIT_FAILURE);
  }
}
#else
argparse::Command& AiArgs::parse(int argc, char* argv[]) {
  try {
    auto& cmd = parser.parse(argc, (const char**)argv);
    return cmd;
  } catch (std::exception const& e) {
    std::cerr << e.what() << "\n";
    exit(EXIT_FAILURE);
  }
}
#endif

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
      .choices({0, 1, 2, 3, 4})
      .choices_description({{"0", "DEBUG"},
                            {"1", "INFO"},
                            {"2", "WARNING"},
                            {"3", "ERROR"},
                            {"4", "FATAL"}});
  parser.add_negative_flag("v", "decrement log level", log_level);
  parser.add_option("enable-logging", "log output to stderr/file", log_type)
      .default_value("stderr")
      .choices({"file", "stderr", "all"});
  parser.add_option("log-file", "log file path", log_file)
      .default_value("debug.log");
  bind_chat_args(parser, *this);
  bind_model_args(parser, *this);
}
