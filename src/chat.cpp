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

int chat(AiArgs const& args) {
    auto& chat_args = args.chat_args;
    try {
        OpenAIClient client(args);

        nlohmann::json chat_history = nlohmann::json::array();

        try {
            auto response = client.chat(chat_args.system_prompt.value_or(""),
                                        chat_args.prompts, chat_history);
            chat_history.push_back(nlohmann::json::object(
                {{"role", "assistant"}, {"content", response.content}}));

            if (!response.reasoning_content.empty()) {
                auto merged_content = "<think>\n" + response.reasoning_content +
                                      "\n</think>\n\n" + response.content;
                save_to_clipboard(merged_content);
                if (!args.chat_args.stream || args.debug) {
                    std::cout << merged_content << std::endl;
                }
            } else {
                save_to_clipboard(response.content);
                if (!args.chat_args.stream || args.debug) {
                    std::cout << response.content << std::endl;
                }
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
