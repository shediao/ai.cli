#include "chat.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <utfx/utfx.hpp>

#include "args.h"
#include "clip.h"
#include "logging.h"
#include "openai.h"
#include "tool_calls.h"
#include "utils.h"

int chat() {
  AiArgs const& args = AiArgs::instance();
  auto& chat_args = args.chat_args;
  try {
    OpenAIClient client;

    nlohmann::json chat_history = nlohmann::json::array();
    auto history_file =
        std::filesystem::path(app_data_dir("ai.cli")) / "chat.history";
    AutoRun scope_exit_runner([&chat_history, &history_file]() {
      std::filesystem::path p{history_file};
      if (!exists(p.parent_path())) {
        std::filesystem::create_directory(p.parent_path());
      }
      write_to_history(chat_history, history_file.string());
    });
    if (chat_args.continue_with_last_history) {
      auto last_history = get_last_history(history_file.string());
      if (last_history.has_value()) {
        chat_history = std::move(last_history.value());
      }
    }

    try {
      std::string system_prompt = chat_args.system_prompt.value_or("");
      auto user_prompt = chat_args.prompts;
      if (!utfx::is_utf8(system_prompt.c_str(), system_prompt.size())) {
        LOG(ERROR) << "system prompt not an utf8 string";
        return 1;
      }
      if (std::find_if_not(user_prompt.begin(), user_prompt.end(),
                           [](std::string const& s) {
                             return utfx::is_utf8(s.data(), s.size());
                           }) != user_prompt.end()) {
        LOG(ERROR) << "user prompt not an utf8 string";
        return 1;
      }
      while (true) {
        auto response = client.chat(system_prompt, user_prompt, chat_history);
        if (!response.has_value()) {
          return 1;
        }

        auto& reasoning_content =
            response.value().choices().back().message.reasoning_content;
        auto& content = response.value().choices().back().message.content;
        auto finish_reason = response.value().choices().back().finish_reason;

        LOG_IF(INFO,
               !response.value().choices().back().message.tool_calls.empty())
            << response.value().choices().back().message.tool_calls_json().dump(
                   2);

        LOG_IF(INFO, !finish_reason.empty())
            << "finish_reason: " << finish_reason;

        if (!args.chat_args.stream) {
          if (!reasoning_content.empty()) {
            auto merged_content = "<thinking>\n" + reasoning_content +
                                  "\n</thinking>\n\n" + content;
            save_to_clipboard(merged_content);
            LOG(INFO) << merged_content;
          } else {
            if (!content.empty()) {
              save_to_clipboard(content);
              std::cout << content << std::endl;
            }
          }
        } else {
          if (!content.empty()) {
            save_to_clipboard(content);
          }
        }

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
              std::cout << "==> " << function << "(" << arguments.dump()
                        << ")\n";
              auto ret = call_tool(function, arguments);
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
