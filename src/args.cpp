#include "ai/args.h"

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

#include "ai/config.h"
#include "ai/system_prompt.h"
#include "ai/tool_calls.h"
#include "ai/utils.h"

namespace ai {

namespace {

// Lazily loaded application config (cached after first access)
static AppConfig& app_config() {
  static AppConfig config = load_config();
  return config;
}

// Look up a provider by alias; returns nullptr if not found
static const ProviderConfig* find_provider(const std::string& alias) {
  return app_config().find_provider(alias);
}

// Get the default model for a provider alias (from config)
static std::optional<std::string> get_provider_default_model(
    const std::string& alias) {
  if (auto* p = find_provider(alias)) {
    return p->default_model;
  }
  return std::nullopt;
}

// Get the API key for a provider alias (from config)
static std::optional<std::string> get_provider_api_key(
    const std::string& alias) {
  if (auto* p = find_provider(alias)) {
    return p->api_key;
  }
  return std::nullopt;
}

// Find the provider alias that matches a URL prefix
static std::string find_alias_for_url(const std::string& url) {
  for (auto& p : app_config().providers) {
    if (url.find(p.base_url) != std::string::npos) {
      return p.alias;
    }
  }
  return "";
}

// Get environment config using the provider's alias as env prefix
static std::optional<std::string> getEnvironmentConfig(
    std::string const& url, std::string const& config_postfix) {
  auto alias = find_alias_for_url(url);
  if (!alias.empty()) {
    std::string env_prefix;
    for (auto& c : alias) {
      env_prefix += static_cast<char>(std::toupper(c));
    }
    return env::get(env_prefix + config_postfix);
  }
  return std::nullopt;
}

// Get default model: first try env var, then config, then hardcoded fallback
static std::optional<std::string> getDefaultModelForUrl(
    std::string const& url) {
  // First try environment variable
  auto env_model = getEnvironmentConfig(url, "_API_MODEL");
  if (env_model.has_value()) {
    return env_model;
  }
  // Then try config's default_model
  auto alias = find_alias_for_url(url);
  if (!alias.empty()) {
    auto config_model = get_provider_default_model(alias);
    if (config_model.has_value()) {
      return config_model;
    }
  }
  return std::nullopt;
}

// Get API key: first try command line, then env var, then config
static void resolve_api_key(AiArgs& args, const std::string& url) {
  if (!args.api_key.empty()) {
    return;  // Already set via command line
  }
  // Try environment variable
  auto env_key = getEnvironmentConfig(url, "_API_KEY");
  if (env_key.has_value()) {
    args.api_key = env_key.value();
    return;
  }
  // Try config
  auto alias = find_alias_for_url(url);
  if (!alias.empty()) {
    auto config_key = get_provider_api_key(alias);
    if (config_key.has_value()) {
      args.api_key = config_key.value();
    }
  }
}

// Get proxy: first try command line, then env var
static void resolve_proxy(AiArgs& args, const std::string& url) {
  if (args.proxy.has_value()) {
    return;  // Already set via command line
  }
  auto proxy = getEnvironmentConfig(url, "_API_PROXY");
  if (proxy.has_value()) {
    args.proxy = proxy.value();
  }
}

static void add_alias_options(argparse::Command& command) {
  for (auto& p : app_config().providers) {
    if (!p.alias.empty() && !p.base_url.empty()) {
      command.add_alias(p.alias, "base-url", p.base_url);
    }
  }
}

static void bind_model_args(argparse::ArgParser& parser, AiArgs& args) {
  auto& models = parser.add_command(
      "models", "List available AI models from the configured API endpoint");

  // Default base URL: use the first configured provider, or deepseek as
  // fallback
  const auto default_base = app_config().providers.empty()
                                ? "https://api.deepseek.com"
                                : app_config().providers.front().base_url;

  auto& models_args = args.models_args;
  models
      .add_option("u,url", "OpenAI API-compatible base URL for listing models",
                  models_args.api_url)
      .hidden();
  models
      .add_option("base-url",
                  "OpenAI API-compatible base URL (appends /models to form the "
                  "full endpoint)",
                  models_args.api_url)
      .default_value(default_base)
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
    resolve_api_key(args, models_args.api_url);
    resolve_proxy(args, models_args.api_url);
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
  auto& chat = parser.add_command(
      "chat", "Start an interactive chat session with the AI assistant");
  auto& chat_args = args.chat_args;

  // Default base URL: use the first configured provider, or deepseek as
  // fallback
  const auto default_base = app_config().providers.empty()
                                ? "https://api.deepseek.com"
                                : app_config().providers.front().base_url;

  chat.add_flag(
          "stream",
          "Enable streaming mode (tokens are displayed as they are generated)",
          chat_args.stream)
      .negatable();
  chat.add_flag("stream-include-usage",
                "Include token usage statistics at the end of streaming output",
                chat_args.stream_include_usage)
      .negatable();

  chat.add_flag("C", "Continue conversation from the last saved chat history",
                chat_args.continue_with_last_history);

  chat.add_option("m,model",
                  "AI model name to use for chat completion (e.g., gpt-4o, "
                  "deepseek-chat)",
                  chat_args.model)
      .validator([](const std::string& model) {
        return std::pair<bool, std::string>{!model.empty(),
                                            "model must be a non-empty string"};
      });
  chat.add_option(
          "s,system-prompt",
          "System prompt that sets the behavior, role, and context for the AI",
          chat_args.system_prompt)
      .default_value(build_default_system_prompt());
  chat.add_option("t,temperature",
                  "Sampling temperature [0.0–2.0]: lower values produce more "
                  "focused/deterministic output, higher values produce more "
                  "creative/varied output",
                  chat_args.temperature)
      .range(0.0, 2.0);
  chat.add_option(
          "top-p",
          "Nucleus sampling parameter [0.0–1.0]: considers only the smallest "
          "set of tokens whose cumulative probability exceeds this value",
          chat_args.top_p)
      .range(0.0, 1.0);
  chat.add_option("u,url",
                  "OpenAI API-compatible base URL for chat completions",
                  chat_args.api_url)
      .hidden();
  chat.add_option("base-url",
                  "OpenAI API-compatible base URL (appends /chat/completions "
                  "to form the full endpoint)",
                  chat_args.api_url)
      .default_value(default_base)
      .callback([&args](std::string const& base_url) {
        if (!base_url.empty()) {
          args.chat_args.api_url =
              base_url + (base_url[base_url.size() - 1] == '/'
                              ? "chat/completions"
                              : "/chat/completions");
        }
      });

  chat.add_option("max-tokens",
                  "Maximum number of tokens to generate in the response",
                  chat_args.max_tokens);
  chat.add_option("reasoning-effort",
                  "Control the model's reasoning depth before responding",
                  chat_args.reasoning_effort)
      .choices({"low", "medium", "high", "none"});

  chat.add_flag("no-tools", "Disable all tool calling capabilities",
                chat_args.no_tools);
  chat.add_option(
          "tools",
          "Tool categories to enable for the AI (e.g., bash, filesystem, git)",
          chat_args.tools)
      .choices([&]() {
        auto cats = get_tool_categories();
        return std::vector<std::string>(cats.begin(), cats.end());
      }())
      .default_value({"bash", "filesystem"});
  chat.add_option(
          "tool-choice",
          "Control how the AI selects and uses tools: 'none' (never call "
          "tools), 'auto' (let the AI decide), 'required' (force a tool call)",
          chat_args.tool_choice)
      .choices({"none", "auto", "required"});

  add_alias_options(chat);

  chat.add_positional(
      "prompts",
      "User message(s) to send to the AI; use '-' to read from stdin",
      chat_args.prompts);

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

    if (chat_args.no_tools) {
      chat_args.tools.clear();
      chat_args.tool_choice = "none";
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

    resolve_api_key(args, chat_args.api_url);
    resolve_proxy(args, chat_args.api_url);
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

AiArgs::AiArgs()
    : parser("ai",
             "OpenAI API-compatible multi-provider CLI chatbot with "
             "tool-calling capabilities") {
  parser.add_flag("version", "Print version information and exit", version)
      .callback([](bool v) {
        if (v) {
          std::cout << "ai version " << GIT_VERSION << "\n";
          std::exit(0);
        }
      });
  parser
      .add_option(
          "x,proxy",
          "HTTP/HTTPS proxy URL for API requests (e.g., http://127.0.0.1:8080)",
          proxy)
      .value_placeholder("PROXY");
  parser
      .add_option("k,key",
                  "API key used for authenticating with the AI provider",
                  api_key)
      .value_placeholder("KEY");
  parser
      .add_option("log-level",
                  "Set logging verbosity level (lower values are more verbose)",
                  log_level)
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
  parser.add_negative_flag(
      "v", "Decrease log verbosity (each use makes output less verbose)",
      log_level);
  parser
      .add_option("enable-logging",
                  "Enable logging and choose output destination", log_type)
      .default_value("stderr")
      .choices({"file", "stderr", "all"});
  parser
      .add_option("log-file",
                  "Path to the log file when file-based logging is enabled",
                  log_file)
      .default_value("debug.log");
  parser
      .add_flag("print-bash-complete", "Print bash completion script",
                print_bash_completion)
      .callback([this](bool v) {
        if (v) {
          this->parser.print_bash_complete(std::cout);
          std::exit(0);
        }
      })
      .hidden();
  parser
      .add_flag("print-zsh-complete", "Print zsh completion script",
                print_zsh_completion)
      .callback([this](bool v) {
        if (v) {
          this->parser.print_zsh_complete(std::cout);
          std::exit(0);
        }
      })
      .hidden();
  parser
      .add_flag("print-fish-complete", "Print fish completion script",
                print_fish_completion)
      .callback([this](bool v) {
        if (v) {
          this->parser.print_fish_complete(std::cout);
          std::exit(0);
        }
      })
      .hidden();
  bind_chat_args(parser, *this);
  bind_model_args(parser, *this);
}

}  // namespace ai
