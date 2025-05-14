#include "./chat.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "./args.h"
#include "./clip.h"
#include "./openai.h"
#include "./stream.h"
#include "./tools_call.h"

int chat() {
    AiArgs const& args = AiArgs::instance();
    auto& chat_args = args.chat_args;
    try {
        OpenAIClient client;

        nlohmann::json chat_history = nlohmann::json::array();

        try {
            std::string system_prompt = chat_args.system_prompt.value_or("");
            auto user_prompt = chat_args.prompts;
            while (true) {
                auto response =
                    client.chat(system_prompt, user_prompt, chat_history);
                auto reasoning_content = response.reasoning_content();
                auto content = response.content();
                auto tool_calls = response.tool_calls();
                auto finish_reason = response.finish_reason();

                if (args.debug && tool_calls.has_value()) {
                    std::cout << tool_calls.value().dump(2) << '\n';
                }
                if (args.debug && finish_reason.has_value()) {
                    std::cout << "finish_reason: " << finish_reason.value()
                              << '\n';
                }

                if (content.has_value() &&
                    !(finish_reason.has_value() &&
                      finish_reason.value() == "tool_calls")) {
                    chat_history.push_back(nlohmann::json::object(
                        {{"role", "assistant"}, {"content", content.value()}}));
                }

                if (reasoning_content.has_value()) {
                    auto merged_content =
                        "<think>\n" + reasoning_content.value() +
                        "\n</think>\n\n" + content.value_or("");
                    save_to_clipboard(merged_content);
                    if (!args.chat_args.stream || args.debug) {
                        std::cout << merged_content << std::endl;
                    }
                } else {
                    if (content.has_value()) {
                        save_to_clipboard(content.value());
                        if (!args.chat_args.stream || args.debug) {
                            std::cout << content.value() << std::endl;
                        }
                    }
                }
                if (tool_calls.has_value() && tool_calls.value().size() > 0) {
                    for (auto const& tool : tool_calls.value()) {
                        if (tool.contains("function") &&
                            tool["function"].is_object()) {
                            auto function = tool["function"];
                            auto arguments = nlohmann::json::parse(
                                function["arguments"].get<std::string>());
                            auto ret = call_tool(
                                function["name"].get<std::string>(), arguments);
                            if (ret.has_value()) {
                                chat_history.push_back(nlohmann::json::object(
                                    {{"role", "tool"},
                                     {"tool_call_id",
                                      tool["id"].get<std::string>()},
                                     {"name", tool["function"]["name"]
                                                  .get<std::string>()},
                                     {"content", ret.value()}}));
                            }
                        }
                    }
                }
                if (!finish_reason.has_value() ||
                    finish_reason.value() != "tool_calls") {
                    break;
                }
                user_prompt.clear();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
