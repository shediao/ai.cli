#include "chat.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "args.h"
#include "clip.h"
#include "logging.h"
#include "openai.h"
#include "tool_calls.h"

int chat() {
  AiArgs const& args = AiArgs::instance();
  auto& chat_args = args.chat_args;
  try {
    OpenAIClient client;

    nlohmann::json chat_history = nlohmann::json::array();
    if (chat_args.continue_with_last_history &&
        std::filesystem::exists("ai-history.json")) {
      std::ifstream history_file{"ai-history.json"};
      if (history_file.is_open()) {
        std::string history_content{
            std::istreambuf_iterator<char>(history_file),
            std::istreambuf_iterator<char>()};
        try {
          auto j = nlohmann::json::parse(history_content);
          if (j.is_array()) {
            chat_history = j;
          }
        } catch (...) {
        }
      }
    }

    try {
      std::string system_prompt = chat_args.system_prompt.value_or("");
      auto user_prompt = chat_args.prompts;
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
        }

        if (!response.value().choices().back().message.tool_calls.empty()) {
          for (auto const& tool_call :
               response.value().choices().back().message.tool_calls) {
            auto ret = call_tool(tool_call.function.name,
                                 json::parse(tool_call.function.arguments));
            if (ret.has_value()) {
              chat_history.push_back(
                  nlohmann::json::object({{"role", "tool"},
                                          {"tool_call_id", tool_call.id},
                                          {"name", tool_call.function.name},
                                          {"content", ret.value()}}));
            }
          }
        }
        if (finish_reason != "tool_calls") {
          break;
        }
        user_prompt.clear();
      }
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
    }

    if (chat_history.is_array() && !chat_history.empty()) {
      std::ofstream history_file("ai-history.json");
      if (history_file.is_open()) {
        std::string history_content = chat_history.dump();
        history_file.write(history_content.data(), history_content.size());
        history_file.flush();
        history_file.close();
      }
    }

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
