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

        std::string prompt = chat_args.prompt;

        try {
            nlohmann::json chat_history = nlohmann::json::array();
            if (chat_args.stream) {
                setbuf(stdout, nullptr);
                std::cout.rdbuf()->pubsetbuf(nullptr, 0);
                std::stringstream eat;
                StreamOperator stream{args.debug ? eat : std::cout};
                client.chat(chat_args.system_prompt.value_or(""), prompt,
                            chat_args.files, chat_history,
                            [&stream, &args](const std::string& chunk) {
                                if (args.debug) {
                                    std::cout << chunk;
                                }
                                stream.parse(std::string_view{chunk.data(),
                                                              chunk.size()});
                            });
                if (!stream.parse_done()) {
                    if (stream.data_lines().empty()) {
                        try {
                            auto x =
                                nlohmann::json::parse(stream.response_data());
                            // TODO:
                            std::cerr << x.dump() << '\n';
                        } catch (...) {
                            std::cerr << std::string_view{stream.response_data()
                                                              .data(),
                                                          stream.response_data()
                                                              .size()}
                                      << '\n';
                        }
                    }
                }
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "user"}, {"content", prompt}}));
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "assistant"}, {"content", stream.content()}}));
                if (stream.reasoning_content().empty()) {
                    save_to_clipboard(stream.content());
                    if (args.debug) {
                        std::cout << stream.content() << '\n';
                    }
                } else {
                    auto merged_content = "<think>\n" +
                                          stream.reasoning_content() +
                                          "\n</think>\n\n" + stream.content();
                    save_to_clipboard(merged_content);
                    if (args.debug) {
                        std::cout << merged_content << '\n';
                    }
                }
            } else {
                auto response =
                    client.chat(chat_args.system_prompt.value_or(""), prompt,
                                chat_args.files, chat_history);
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "user"}, {"content", prompt}}));
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "assistant"}, {"content", response.content}}));

                if (!response.reasoning_content.empty()) {
                    auto merged_content = "<think>\n" +
                                          response.reasoning_content +
                                          "\n</think>\n\n" + response.content;
                    save_to_clipboard(merged_content);
                    std::cout << merged_content << std::endl;
                } else {
                    save_to_clipboard(response.content);
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
