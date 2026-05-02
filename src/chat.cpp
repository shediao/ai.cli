#include "ai/chat.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <utfx/utfx.hpp>

#include "ai/args.h"
#include "ai/history.h"
#include "ai/logging.h"
#include "ai/openai.h"
#include "ai/system_prompt.h"
#include "ai/tool_calls.h"
#include "ai/utils.h"

using json = nlohmann::json;

namespace ai {

int chat() {
  AiArgs const& args = AiArgs::instance();
  auto& chat_args = args.chat_args;
  try {
    OpenAIClient client;

    nlohmann::json chat_history = nlohmann::json::array();
    auto history_db_path =
        std::filesystem::path(app_data_dir("ai.cli")) / "chat_history.db";
    HistoryDB history_db(history_db_path.string());
    std::string session_id;

    if (chat_args.continue_with_last_history) {
      auto last_history = history_db.get_last_messages();
      if (last_history.has_value()) {
        chat_history = std::move(last_history.value());
        LOG(INFO) << "Continuing from last messages (" << chat_history.size()
                  << " messages)";
      }
    }

    // Always create a session so we have a session_id for saving.
    // If we loaded previous messages, create_session() starts a new
    // session that inherits the loaded chat_history via save_messages().
    session_id = history_db.create_session();

    AutoRun scope_exit_runner([&chat_history, &history_db, &session_id]() {
      history_db.save_messages(session_id, chat_history);
    });

    try {
      std::string system_prompt = chat_args.system_prompt.has_value()
                                      ? chat_args.system_prompt.value()
                                      : build_default_system_prompt();
      auto user_prompt = chat_args.prompts;
      if (chat_args.system_prompt.has_value() &&
          !utfx::is_utf8(system_prompt.c_str(), system_prompt.size())) {
        LOG(ERROR) << "system prompt not an utf8 string";
        return 1;
      }

      LOG(INFO) << "system prompt: " << system_prompt;
#if defined(_WIN32)
      for (auto& s : user_prompt) {
        if (!utfx::is_utf8(s.data(), s.size())) {
          auto u8 = toUtf8(s);
          if (!u8) {
            LOG(ERROR) << "user prompt not an utf8 string";
            return 1;
          }
          s = u8.value();
        }
      }
#else
      if (auto it = std::find_if_not(user_prompt.begin(), user_prompt.end(),
                                     [](std::string const& s) {
                                       return utfx::is_utf8(s.data(), s.size());
                                     });
          it != user_prompt.end()) {
        LOG(ERROR) << "user prompt not an utf8 string";
        return 1;
      }
#endif
      while (true) {
        auto response = client.chat(system_prompt, user_prompt, chat_history);
        if (!response.has_value()) {
          return 1;
        }

        auto& reasoning_content =
            response.value().choices().back().message.reasoning_content;
        auto& content = response.value().choices().back().message.content;
        auto finish_reason = response.value().choices().back().finish_reason;

        if (!args.chat_args.stream) {
          if (reasoning_content.has_value()) {
            auto merged_content = "<thinking>\n" +
                                  reasoning_content.value_or("") +
                                  "\n</thinking>\n\n" + content;
            LOG(INFO) << merged_content;
            std::cout << merged_content << std::endl;
          } else {
            if (!content.empty()) {
              std::cout << content << std::endl;
              LOG(INFO) << content;
            }
          }
        } else {
          LOG_IF(INFO, reasoning_content.has_value())
              << "<thinking>\n" + reasoning_content.value_or("") +
                     "\n</thinking>\n\n" + content;
          LOG_IF(INFO, !content.empty()) << content;
        }

        LOG_IF(INFO,
               !response.value().choices().back().message.tool_calls.empty())
            << response.value().choices().back().message.tool_calls_json().dump(
                   2);

        LOG_IF(INFO, !finish_reason.empty())
            << "finish_reason: " << finish_reason;

        if (response.value().choices().back().message.tool_calls.empty() &&
            finish_reason == "tool_calls") {
          LOG(FATAL) << "not found tool_calls";
        }

        if (!response.value().choices().back().message.tool_calls.empty()) {
          for (auto const& tool_call :
               response.value().choices().back().message.tool_calls) {
            try {
              auto function = tool_call.function.name;
              auto arguments = json::parse(tool_call.function.arguments);
              auto now = std::chrono::system_clock::now();
              auto time_t_now = std::chrono::system_clock::to_time_t(now);
#if defined(_WIN32)
              struct tm tm;
              localtime_s(&tm, &time_t_now);
#else
              auto tm = *std::localtime(&time_t_now);
#endif
              char buf[64];
              std::strftime(buf, sizeof(buf), "[%Y/%m/%d %H:%M:%S]", &tm);
              std::cout << "\n\033[32;1m ● " << buf << " `"
                        << function + "(" + arguments.dump() + ")"
                        << "`\033[0m\n";
              auto ret = call_tool(function, arguments);
              if (ret.size() > 120) {
                std::cout << "\033[33;1m" << ret.substr(0, 114)
                          << "......\033[0m\n";
              } else {
                std::cout << "\033[33;1m" << ret << "\033[0m\n";
              }
              chat_history.push_back(
                  nlohmann::json::object({{"role", "tool"},
                                          {"tool_call_id", tool_call.id},
                                          {"name", function},
                                          {"content", ret}}));

            } catch (json::parse_error const& e) {
              LOG(ERROR) << tool_call.function.name << "("
                         << tool_call.function.arguments << ")" << e.what();
            } catch (std::exception const& e) {
              LOG(ERROR) << tool_call.function.name << "("
                         << tool_call.function.arguments << ")" << e.what();
              LOG(ERROR) << e.what();
            }
          }
        }
        if (finish_reason != "tool_calls") {
          break;
        }
        user_prompt.clear();
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << e.what();
      return 1;
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}

}  // namespace ai
