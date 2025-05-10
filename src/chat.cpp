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

std::optional<std::string> get_stream_context(const std::string_view data,
                                              bool& pre_is_reasoning) {
    try {
        nlohmann::json j = nlohmann::json::parse(data);
        if (j.contains("choices") && j["choices"].is_array() &&
            j["choices"].size() > 0 && j["choices"][0].contains("delta")) {
            auto& delta = j["choices"][0]["delta"];
            if (delta.contains("content") && delta["content"].is_string()) {
                auto content = delta["content"].get<std::string>();
                auto ret =
                    pre_is_reasoning ? ("\n<结束思考>\n" + content) : content;
                pre_is_reasoning = false;
                return ret;
            } else if (delta.contains("reasoning_content") &&
                       delta["reasoning_content"].is_string()) {
                auto content = delta["reasoning_content"].get<std::string>();
                auto ret =
                    pre_is_reasoning ? content : ("\n<开始思考>\n" + content);
                pre_is_reasoning = true;
                return ret;
            }
        }
        if (j.contains("choices") && j["choices"].is_array() &&
            j["choices"].size() == 0 && j.contains("usage")) {
            return "\n\n" + j["usage"].dump();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n```\n"
                  << data << "\n```" << std::endl;
    }
    return std::nullopt;
}

constexpr std::string_view data_prefix = "data: ";
constexpr std::string_view event_prefix = "event: ";
constexpr std::string_view event_error_prefix = "event: error";
constexpr std::string_view done_prefix = "[DONE]";

std::optional<std::string> get_new_line(std::string& unparsed_json) {
    auto begin = std::find_if(unparsed_json.begin(), unparsed_json.end(),
                              [](char c) { return c != '\n'; });
    if (begin == unparsed_json.end()) {
        return std::nullopt;
    }
    auto newline_iter = std::find(begin, unparsed_json.end(), '\n');
    if (newline_iter == unparsed_json.end()) {
        return std::nullopt;
    }
    std::string_view line{begin, newline_iter};
    if (line.starts_with(data_prefix) || line.starts_with(event_prefix) ||
        line.starts_with(": keep-alive")) {
        std::string result = std::string(line);
        unparsed_json.erase(unparsed_json.begin(), newline_iter + 1);
        return result;
    }
    return std::nullopt;
}

void process_stream_output(const std::string& chunk, std::ostream& os,
                           std::string& unparsed_json,
                           ResponseContent& response, bool& pre_is_reasoning) {
    unparsed_json += chunk;
    while (true) {
        auto line = get_new_line(unparsed_json);
        if (!line) {
            break;
        }
        if (line->starts_with(data_prefix)) {
            // If line starts with "data: "
            std::string_view data{line->data() + data_prefix.size(),
                                  line->size() - data_prefix.size()};
            if (data.starts_with(done_prefix)) {
                // If line starts with "[DONE]", stream ends
                break;
            }
            if (auto context = get_stream_context(data, pre_is_reasoning);
                context) {
                os << *context;
                if (pre_is_reasoning) {
                    response.reasoning_content += *context;
                } else {
                    response.content += *context;
                }
            }
        } else if (line->starts_with(": keep-alive")) {
            continue;
        } else if (line->starts_with(event_error_prefix)) {
            std::cerr << "Error: " << *line << std::endl;
            break;
        } else if (line->starts_with(event_prefix)) {
            // 获取event
            continue;
        } else {
            // 遇到可能是错误信息，直接退出当前处理
            break;
        }
    }
}

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
                std::string unparsed_json;
                std::stringstream response_stream;
                ResponseContent response;

                bool pre_is_reasoning = false;
                client.chat(
                    chat_args.system_prompt.value_or(""), prompt,
                    chat_args.files, chat_history,
                    [&unparsed_json, &response, &args, &pre_is_reasoning,
                     &response_stream](const std::string& chunk) {
                        if (args.debug) {
                            std::cout << chunk;
                        }
                        process_stream_output(
                            chunk, args.debug ? response_stream : std::cout,
                            unparsed_json, response, pre_is_reasoning);
                    });
                if (!unparsed_json.empty() &&
                    unparsed_json.find("error") != std::string::npos) {
                    std::cerr << unparsed_json;
                }
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "user"}, {"content", prompt}}));
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "assistant"}, {"content", response.content}}));
                if (response.reasoning_content.empty()) {
                    save_to_clipboard(response.content);
                    if (args.debug) {
                        std::cout << response.content << '\n';
                    }
                } else {
                    auto merged_content = "<think>\n" +
                                          response.reasoning_content +
                                          "\n</think>\n\n" + response.content;
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
