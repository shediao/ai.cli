#include "ai/args.h"

#include <algorithm>
#include <argparse/argparse.hpp>
#include <cstdlib>
#include <environment/environment.hpp>
#include <limits>

#include "ai/terminal.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "ai/config.h"
#include "ai/function.h"
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
static std::optional<std::string> resolve_api_key(const std::string& url) {
  // Try environment variable
  auto env_key = getEnvironmentConfig(url, "_API_KEY");
  if (env_key.has_value()) {
    return env_key.value();
  }
  // Try config
  auto alias = find_alias_for_url(url);
  if (!alias.empty()) {
    auto config_key = get_provider_api_key(alias);
    if (config_key.has_value()) {
      return config_key.value();
    }
  }
  return std::nullopt;
}

// Get proxy: first try command line, then env var
static std::optional<std::string> resolve_proxy(const std::string& url) {
  auto proxy = getEnvironmentConfig(url, "_API_PROXY");
  if (proxy.has_value()) {
    return proxy.value();
  }
  return std::nullopt;
}

static void add_alias_options(argparse::Command& command) {
  for (auto& p : app_config().providers) {
    if (!p.alias.empty() && !p.base_url.empty() &&
        std::all_of(p.alias.begin(), p.alias.end(), [](char c) {
          return std::isalnum(c) || c == '-' || c == '_' || c == '.';
        })) {
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
    if (args.api_key.empty()) {
      args.api_key = resolve_api_key(models_args.api_url).value_or("");
    }
    if (!args.proxy.has_value()) {
      auto proxy = resolve_proxy(models_args.api_url);
      if (proxy.has_value()) {
        args.proxy = proxy.value();
      }
    }
  });
}

inline static bool stdin_is_pipe() {
#ifdef _WIN32
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
#ifdef _WIN32
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
          "Enable streaming mode (tokens are displayed as they are generated). "
          "Defaults to true when output is a terminal, false otherwise.",
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
      chat_args.system_prompt);
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
      .value_placeholder("EFFORT")
      .choices({"low", "medium", "high", "none"});

  chat.add_flag("thinking", "Enable or disable AI thinking", chat_args.thinking)
      .is_negatable();

  chat.add_option("topic-base-url",
                  "OpenAI API-compatible base URL for topic generation "
                  "(appends /chat/completions to form the full endpoint; "
                  "defaults to chat base-url if not set)",
                  chat_args.topic_base_url)
      .value_placeholder("URL")
      .callback([&chat_args](std::string const& base_url) {
        if (!base_url.empty()) {
          chat_args.topic_base_url =
              base_url + (base_url[base_url.size() - 1] == '/'
                              ? "chat/completions"
                              : "/chat/completions");
        }
      })
      .hidden();
  chat.add_option("topic-api-key",
                  "API key for topic generation "
                  "(defaults to chat API key if not set)",
                  chat_args.topic_api_key)
      .value_placeholder("KEY")
      .hidden();
  chat.add_option("topic-model",
                  "Model to use for topic generation "
                  "(defaults to chat model if not set)",
                  chat_args.topic_model)
      .value_placeholder("MODEL")
      .hidden();

  chat.add_flag("no-tools", "Disable all tool calling capabilities",
                chat_args.no_tools);
  chat.add_flag("list-tools",
                "List all available tool categories and their functions",
                chat_args.list_tools);
  chat.add_option(
          "tools",
          "Tool categories to enable for the AI (e.g., bash, filesystem)",
          chat_args.tools)
      .value_placeholder("TOOL")
      .choices([&]() {
        auto cats = get_tool_categories();
        std::map<std::string, std::string> choices;
        for (const auto& cat : cats) {
          choices[cat] = cat;
        }
        return choices;
      }());
  chat.add_option(
          "tool-choice",
          "Control how the AI selects and uses tools: 'none' (never call "
          "tools), 'auto' (let the AI decide), 'required' (force a tool call)",
          chat_args.tool_choice)
      .value_placeholder("HOW")
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
    } else {
      if (chat_args.tools.empty()) {
        chat_args.tools.insert("default");
        chat_args.tools.insert("filesystem");
#if defined(_WIN32)
        if (auto shell = env::get("SHELL");
            shell.has_value() && (shell.value().ends_with("bash") ||
                                  shell.value().ends_with("bash.exe"))) {
          chat_args.tools.insert("bash");
        } else if (auto paths = env::path();
                   std::any_of(paths.begin(), paths.end(), [](auto const& p) {
                     return exists(std::filesystem::path(p) / "bash.exe");
                   })) {
          chat_args.tools.insert("bash");
        }
        chat_args.tools.insert("cmd");
        chat_args.tools.insert("powershell");
#else
        chat_args.tools.insert("bash");
#endif
      }
    }

    if (chat_args.list_tools) {
      for (auto const& category : get_tool_categories()) {
        auto schema_str = get_tool_schema(category);
        if (schema_str.empty()) continue;
        try {
          auto schema = nlohmann::json::parse(schema_str);
          std::cout << term::bold << "[" << category << "]" << term::reset
                    << "\n";
          for (auto const& tool : schema) {
            std::cout << "  " << term::bold_color::cyan
                      << tool.value("name", "???") << term::reset << ": "
                      << tool.value("description", "") << "\n";
          }
          std::cout << "\n";
        } catch (...) {
          std::cout << term::bold << "[" << category << "]" << term::reset
                    << " (parse error)\n\n";
        }
      }
      std::exit(0);
    }

    // read from stdin
    if (auto it =
            std::find(begin(chat_args.prompts), end(chat_args.prompts), "-");
        it != end(chat_args.prompts)) {
      *it = std::string(std::istreambuf_iterator<char>(std::cin),
                        std::istreambuf_iterator<char>{});
    };

    if (chat_args.prompts.empty() && utils::stdin_is_atty()) {
      try {
        if (auto prompt = ai::utils::getUserInputViaEditor(); !prompt.empty()) {
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

    if (args.api_key.empty()) {
      args.api_key = resolve_api_key(chat_args.api_url).value_or("");
    }

    if (!args.proxy.has_value()) {
      auto proxy = resolve_proxy(chat_args.api_url);
      if (proxy.has_value()) {
        args.proxy = proxy.value();
      }
    }
    if (chat_args.topic_base_url.has_value()) {
      bool same_api = (chat_args.topic_base_url == chat_args.api_url);

      if (!chat_args.topic_model.has_value()) {
        if (same_api) {
          chat_args.topic_model = chat_args.model;
        } else {
          chat_args.topic_model =
              getDefaultModelForUrl(chat_args.topic_base_url.value());
        }
      }
      if (!chat_args.topic_model.has_value()) {
        std::cerr << "Error: Must provide a topic model." << std::endl;
        exit(EXIT_FAILURE);
      }

      if (!chat_args.topic_api_key.has_value()) {
        if (same_api) {
          chat_args.topic_api_key = args.api_key;
        } else {
          chat_args.topic_api_key =
              resolve_api_key(chat_args.topic_base_url.value());
        }
      }
    }
  });
}

static void bind_history_args(argparse::ArgParser& parser, AiArgs& args) {
  auto& history =
      parser.add_command("history", "List recent chat session history");
  auto& history_args = args.history_args;

  history
      .add_option("n", "Number of recent sessions to list (0 or omit for all)",
                  history_args.n)
      .range(-1, std::numeric_limits<int>::max())
      .default_value("1");
  history
      .add_option("format",
                  "Output format for history listing: 'text' (human-readable) "
                  "or 'json'",
                  history_args.format)
      .value_placeholder("FORMAT")
      .default_value("text")
      .choices({"json", "text"});
  history.callback([]() -> void {
    // nothing to resolve for history command
  });
}

static void bind_update_args(argparse::ArgParser& parser, AiArgs& args) {
  auto& update = parser.add_command(
      "update",
      "Check for a newer version on GitHub and self-update if available");
  auto& update_args = args.update_args;

  update.add_flag("f,force",
                  "Force update even if the current version is already the "
                  "latest",
                  update_args.force);
}

static void bind_ai_args(argparse::ArgParser& parser, AiArgs& args) {
  parser
      .add_flag("v,version", "Print version information and exit", args.version)
      .callback([](bool v) {
        if (v) {
          std::cout << "ai version " << GIT_VERSION
#ifdef __SANITIZE_ADDRESS__
                    << " (asan)"
#else
#ifdef __has_feature
#if __has_feature(address_sanitizer)
                    << " (asan)"
#endif
#endif
#endif
                    << "\n";
          std::exit(0);
        }
      });
  parser
      .add_option(
          "x,proxy",
          "HTTP/HTTPS proxy URL for API requests (e.g., http://127.0.0.1:8080)",
          args.proxy)
      .value_placeholder("PROXY");
  parser
      .add_option("k,key",
                  "API key used for authenticating with the AI provider",
                  args.api_key)
      .value_placeholder("KEY");
  parser
      .add_option("log-level",
                  "Set logging verbosity level (lower values are more verbose)",
                  args.log_level)
      .value_placeholder("LEVEL")
#if defined(NDEBUG)
      .default_value("4")
#else
      .default_value("3")
#endif
      .choices({{0, "DEBUG"},
                {1, "INFO"},
                {2, "WARNING"},
                {3, "ERROR"},
                {4, "FATAL"}});
  parser.add_alias("verbose", "log-level", "-1");
  parser.add_alias("debug", "log-level", "0");
  parser.add_alias("info", "log-level", "1");
  parser.add_alias("warn", "log-level", "2");
  parser.add_alias("error", "log-level", "3");
  parser.add_alias("fatal", "log-level", "4");
  parser.add_option("log-to", "Path to the log file", args.log_file);
  parser
      .add_flag("print-bash-complete", "Print bash completion script",
                args.print_bash_completion)
      .callback([&parser](bool v) {
        if (v) {
          parser.print_bash_complete(std::cout);
          std::exit(0);
        }
      })
      .hidden();
  parser
      .add_flag("print-zsh-complete", "Print zsh completion script",
                args.print_zsh_completion)
      .callback([&parser](bool v) {
        if (v) {
          parser.print_zsh_complete(std::cout);
          std::exit(0);
        }
      })
      .hidden();
  parser
      .add_flag("print-fish-complete", "Print fish completion script",
                args.print_fish_completion)
      .callback([&parser](bool v) {
        if (v) {
          parser.print_fish_complete(std::cout);
          std::exit(0);
        }
      })
      .hidden();
  bind_chat_args(parser, args);
  bind_model_args(parser, args);
  bind_history_args(parser, args);
  bind_update_args(parser, args);

  parser.usage_footer("\nConfig file:\n  " + config_file_path() + "\n");
  parser.callback([&args]() -> void {
    ::ai::logging::SetLogLevel(args.log_level);
    if (args.log_file.has_value()) {
      ::ai::logging::SetLogFilePath(args.log_file.value());
    }
  });
}

}  // namespace

argparse::ArgParser get_parser(AiArgs& args) {
  argparse::ArgParser parser(
      "ai",
      "OpenAI API-compatible multi-provider CLI chatbot with "
      "tool-calling capabilities");

  bind_ai_args(parser, args);
  return parser;
}

}  // namespace ai
