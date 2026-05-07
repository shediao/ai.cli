#include "ai/chat.h"

#include <ctime>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <utfx/utfx.hpp>

#include "ai/args.h"
#include "ai/history.h"
#include "ai/logging.h"
#include "ai/openai.h"
#include "ai/system_prompt.h"
#include "ai/terminal.h"
#include "ai/tool_calls.h"
#include "ai/utils.h"

using json = nlohmann::json;

namespace ai {

int chat(AiArgs const& args) {
  auto& chat_args = args.chat_args;
  try {
    OpenAIClient client(args);

    nlohmann::json chat_history = nlohmann::json::array();
    HistoryDB history_db(HistoryDB::default_db_path());

    std::optional<HistoryDB::SessionInfo> last_session{std::nullopt};
    if (chat_args.continue_with_last_history) {
      auto last_sessions = history_db.list_session_infos(1);
      if (!last_sessions.empty()) {
        last_session = std::move(last_sessions[0]);
        try {
          chat_history = nlohmann::json::parse(last_session.value().messages);
          LOG(INFO) << "Continuing from last messages (" << chat_history.size()
                    << " messages)";
        } catch (nlohmann::json::parse_error const& e) {
          LOG(ERROR) << "Failed to parse last messages, starting fresh: "
                     << e.what();
          last_session.reset();
          chat_history = nlohmann::json::array();
        }
      }
    }

    // Determine session_id early so we can use it for topic generation
    std::string session_id;
    if (last_session.has_value()) {
      session_id = last_session.value().session_id;
    } else {
      session_id = history_db.create_session();
    }

    // Async topic generation state
    std::future<std::string> topic_future;
    int assistant_count = 0;

    // Count existing assistant messages (relevant for --continue)
    for (auto const& msg : chat_history) {
      if (msg.is_object() && msg.value("role", "") == "assistant") {
        ++assistant_count;
      }
    }

    ai::utils::AutoRun scope_exit_runner(
        [&chat_history, &history_db, &session_id, &topic_future]() {
          history_db.save_messages(session_id, chat_history);

          std::string latest_topic;
          // Best-effort: collect topic from async generation (500ms timeout)
          if (topic_future.valid()) {
            auto status = topic_future.wait_for(std::chrono::milliseconds(500));
            if (status == std::future_status::ready) {
              try {
                latest_topic = topic_future.get();
              } catch (std::exception const& e) {
                LOG(ERROR) << "Async topic generation failed: " << e.what();
              } catch (...) {
                // Ignore any other exceptions
              }
            }
          }

          std::cout << "\n[TOPIC]: " << latest_topic << '\n';
          if (!latest_topic.empty()) {
            try {
              history_db.set_topic(session_id, latest_topic);
            } catch (std::exception const& e) {
              LOG(ERROR) << "Failed to set topic: " << e.what();
            }
          }
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
          auto u8 = ai::utils::toUtf8(s);
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

        // Increment assistant message counter and trigger async topic
        // generation at exponential-backoff checkpoints (2, 7, 13, 19, ...).
        ++assistant_count;
        if (assistant_count == 2 || assistant_count == 7 ||
            assistant_count == 13 || assistant_count == 19) {
          // Only launch a new task when the previous one has finished
          if (!topic_future.valid() ||
              topic_future.wait_for(std::chrono::seconds(0)) ==
                  std::future_status::ready) {
            // Snapshot chat_history to avoid data races with the main loop
            auto history_snapshot = chat_history;
            topic_future =
                std::async(std::launch::async,
                           [history_snapshot = std::move(history_snapshot)]() {
                             return HistoryDB::generate_topic(history_snapshot);
                           });
          }
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
              std::cout << "\n"
                        << term::bold_color::green << " ● ["
                        << ai::utils::timestamp() << "] `"
                        << function + "(" + arguments.dump() + ")" << "`"
                        << term::reset << "\n";
              auto ret = call_tool(function, arguments);
              if (ret.size() > 120) {
                std::cout << term::bold_color::yellow << ret.substr(0, 114)
                          << "......" << term::reset << "\n";
              } else {
                std::cout << term::bold_color::yellow << ret << term::reset
                          << "\n";
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
