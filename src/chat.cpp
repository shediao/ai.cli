#include "./chat.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "./args.h"
#include "./clip.h"
#include "./openai.h"
#include "./tool_calls.h"

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
        auto& tool_calls =
            response.value().choices().back().message.tool_calls_json;
        auto finish_reason = response.value().choices().back().finish_reason;

        if (args.debug && tool_calls) {
          std::cout << tool_calls->dump(2) << '\n';
        }
        if (args.debug && !finish_reason.empty()) {
          std::cout << "finish_reason: " << finish_reason << '\n';
        }

        if (!reasoning_content.empty()) {
          auto merged_content =
              "<think>\n" + reasoning_content + "\n</think>\n\n" + content;
          save_to_clipboard(merged_content);
          if (!args.chat_args.stream || args.debug) {
            std::cout << merged_content << std::endl;
          }
        } else {
          if (!content.empty()) {
            save_to_clipboard(content);
            if (!args.chat_args.stream || args.debug) {
              std::cout << content << std::endl;
            }
          }
        }
        if (tool_calls && tool_calls->size() > 0) {
          for (auto const& tool : *tool_calls) {
            if (tool.contains("function") && tool["function"].is_object()) {
              auto function = tool["function"];
              auto arguments = nlohmann::json::parse(
                  function["arguments"].get<std::string>());
              auto ret =
                  call_tool(function["name"].get<std::string>(), arguments);
              if (ret.has_value()) {
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "tool"},
                     {"tool_call_id", tool["id"].get<std::string>()},
                     {"name", tool["function"]["name"].get<std::string>()},
                     {"content", ret.value()}}));
              }
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
