#include "ai/chat.h"

#include <ctime>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <utfx/utfx.hpp>

#include "ai/args.h"
#include "ai/function.h"
#include "ai/history.h"
#include "ai/logging.h"
#include "ai/openai.h"
#include "ai/system_prompt.h"
#include "ai/terminal.h"
#include "ai/utils.h"
#include "base/scope_exit.h"

using json = nlohmann::json;

namespace ai {

int chat(AiArgs const& args) {
  auto& chat_args = args.chat_args;
  auto start_ts = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
  try {
    OpenAIClient client(args);

    nlohmann::json chat_history = nlohmann::json::array();
    HistoryDB history_db(HistoryDB::default_db_path());

    std::optional<HistoryDB::SessionInfo> continued_session{std::nullopt};
    if (chat_args.continue_with_history_id.has_value()) {
      auto messages_opt =
          history_db.get_messages(chat_args.continue_with_history_id.value());
      if (messages_opt.has_value()) {
        chat_history = messages_opt.value();
        continued_session = HistoryDB::SessionInfo{};
        continued_session->session_id =
            chat_args.continue_with_history_id.value();
        LOG(INFO) << "Continuing from session "
                  << chat_args.continue_with_history_id.value() << " ("
                  << chat_history.size() << " messages)";
      } else {
        LOG(ERROR) << "Session " << chat_args.continue_with_history_id.value()
                   << " not found, starting fresh";
        chat_history = nlohmann::json::array();
      }
    } else if (chat_args.continue_with_last_history) {
      auto recent_sessions = history_db.list_session_infos(1);
      if (!recent_sessions.empty()) {
        continued_session = std::move(recent_sessions[0]);
        try {
          chat_history =
              nlohmann::json::parse(continued_session.value().messages);
          LOG(INFO) << "Continuing from last messages (" << chat_history.size()
                    << " messages)";
        } catch (nlohmann::json::parse_error const& e) {
          LOG(ERROR) << "Failed to parse last messages, starting fresh: "
                     << e.what();
          continued_session.reset();
          chat_history = nlohmann::json::array();
        }
      }
    }

    std::tuple<int, int, int, int, int> tokens = {0, 0, 0, 0, 0};
    auto& [total_tokens, prompt_tokens, completion_tokens,
           prompt_cache_hit_tokens, prompt_cache_miss_tokens] = tokens;

    std::string work_dir = std::filesystem::current_path().string();

    ai::base::scope_exit scope_exit_runner([&chat_history, &history_db,
                                            &continued_session, &args,
                                            &work_dir, &tokens, start_ts]() {
      auto& [total_tokens, prompt_tokens, completion_tokens,
             prompt_cache_hit_tokens, prompt_cache_miss_tokens] = tokens;
      auto end_ts = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
      std::string parent_id;
      if (continued_session.has_value()) {
        parent_id = continued_session.value().session_id;
      }
      std::string session_id = history_db.create_session(
          chat_history, args.chat_args.api_url, args.chat_args.model, work_dir,
          parent_id, start_ts, end_ts, prompt_tokens, completion_tokens,
          total_tokens, prompt_cache_hit_tokens, prompt_cache_miss_tokens);
      auto chat_history_snapshot = chat_history;
      auto topic = HistoryDB::generate_topic(chat_history_snapshot, args);
      std::cout << term::bright_black << "\n[TOPIC]: " << topic << term::reset
                << "\n";
      std::cout << term::bright_black << "\nTokens: [prompt:" << prompt_tokens
                << ", completion:" << completion_tokens
                << ", total:" << total_tokens
                << ", cache_hit:" << prompt_cache_hit_tokens
                << ", cache_miss:" << prompt_cache_miss_tokens << "]\n"
                << term::reset;
      if (!topic.empty()) {
        history_db.set_topic(session_id, topic);
      }
    });

    try {
      auto user_prompt = chat_args.prompts;
      for (auto const& sp : chat_args.system_prompt) {
        if (!utfx::is_utf8(sp.data(), sp.size())) {
          LOG(ERROR) << "system prompt is not a valid UTF-8 string";
          return 1;
        }
      }

#if defined(_WIN32)
      for (auto& s : user_prompt) {
        if (!utfx::is_utf8(s.data(), s.size())) {
          auto u8 = ai::utils::toUtf8(s);
          if (!u8) {
            LOG(ERROR) << "user prompt is not a valid UTF-8 string";
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
        LOG(ERROR) << "user prompt is not a valid UTF-8 string";
        return 1;
      }
#endif

      std::string system_prompt;
      if (!chat_args.system_prompt.empty()) {
        for (size_t i = 0; i < chat_args.system_prompt.size(); ++i) {
          if (i > 0) {
            system_prompt += "\n\n";
          }
          system_prompt += chat_args.system_prompt[i];
        }
      } else {
        if (chat_history.empty()) {
          system_prompt = build_default_system_prompt();
        }
      }
      if (!chat_args.tools.empty() && !system_prompt.empty()) {
        system_prompt += "\n\nWorking Directory: ";
        system_prompt += work_dir;
      }
      LOG(INFO) << "system prompt: "
                << (system_prompt.empty() ? "(preserved from history)"
                                          : system_prompt);
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

        auto const& usage = response.value().usage();
        prompt_tokens += usage.prompt_tokens;
        completion_tokens += usage.completion_tokens;
        total_tokens += usage.total_tokens;
        prompt_cache_hit_tokens += usage.prompt_cache_hit_tokens;
        prompt_cache_miss_tokens += usage.prompt_cache_miss_tokens;

        LOG_IF(INFO,
               !response.value().choices().back().message.tool_calls.empty())
            << response.value().choices().back().message.tool_calls_json().dump(
                   2);

        LOG_IF(INFO, !finish_reason.empty())
            << "finish_reason: " << finish_reason;

        if (response.value().choices().back().message.tool_calls.empty() &&
            finish_reason == "tool_calls") {
          LOG(FATAL) << "tool_calls not found in response";
        }

        if (!response.value().choices().back().message.tool_calls.empty()) {
          for (auto const& tool_call :
               response.value().choices().back().message.tool_calls) {
            try {
              auto function = tool_call.function.name;
              auto arguments = json::parse(tool_call.function.arguments);
              LOG(INFO) << function + "(" + arguments.dump() + ")";
              auto start = std::chrono::steady_clock::now();
              auto ret = call_tool(function, arguments);
              auto elapsed = std::chrono::steady_clock::now() - start;
              auto elapsed_ms =
                  std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                      .count();
              if (ret.size() > 160) {
                std::cout << term::bold_color::yellow
                          << ai::utils::utf8_truncate(ret, 128) << "......"
                          << term::reset << "\n";
              } else {
                std::cout << term::bold_color::yellow << ret << term::reset
                          << "\n";
              }
              std::cout << "[Done] " << function << " took: " << elapsed_ms
                        << "ms, result length: " << ret.size() << "\n";
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
        if (finish_reason != "stop" && finish_reason != "tool_calls") {
          if (finish_reason == "content_filter") {
            LOG(ERROR)
                << "Output content was filtered due to content filter policy.";
            return 1;
          }
          if (finish_reason == "length") {
            LOG(ERROR)
                << "Output length reached the model context length limit, "
                   "or the max_tokens limit.";
            return 1;
          }
          if (finish_reason == "insufficient_system_resources") {
            LOG(ERROR) << "Request was interrupted due to insufficient backend "
                          "inference resources.";
            return 1;
          }
          LOG(WARNING) << "Unknown finish_reason: " << finish_reason;
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
