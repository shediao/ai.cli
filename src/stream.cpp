
#include "./stream.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "openai.h"

constexpr std::string_view data_prefix = "data: ";
constexpr std::string_view done_prefix = "[DONE]";

StreamOperator::StreamOperator(std::ostream& out) : out_{out} {
    response_.choices_.push_back(ResponseContent::Choice{});
}
StreamOperator::StreamOperator() : StreamOperator(std::cout) {}

std::optional<std::string> StreamOperator::getLine() {
    auto begin = std::find_if(response_.response_body_.begin() + parse_index_,
                              response_.response_body_.end(),
                              [](char c) { return c != '\n'; });
    if (begin == response_.response_body_.end()) {
        return std::nullopt;
    }
    auto newline_iter = std::find(begin, response_.response_body_.end(), '\n');
    if (newline_iter == response_.response_body_.end()) {
        return std::nullopt;
    }

    parse_index_ = newline_iter - response_.response_body_.begin() + 1;
    std::string ret{begin, newline_iter};
    return ret;
}

/*
 * data:
 * {"choices":[{"delta":{"role":"assistant","tool_calls":[{"function":{"arguments":"{\"path\":\"src/main.cpp\"}","name":"read_file"},"id":"","type":"function"}]},"finish_reason":"tool_calls","index":0}],"created":1747103222,"model":"gemini-2.5-pro-exp-03-25","object":"chat.completion.chunk"}
 * */
void StreamOperator::parse(std::string_view chunk) {
    if (is_debug) {
        std::cout << chunk;
    }
    response_.response_body_.insert(response_.response_body_.end(),
                                    begin(chunk), end(chunk));
    while (true) {
        auto line = getLine();
        if (!line.has_value()) {
            break;
        }
        if (line.value().starts_with("event: ") ||
            line.value().starts_with(": keep-alive")) {
            continue;
        }
        if (!line.value().starts_with(data_prefix)) {
            break;
        }
        // If line starts with "data: "
        auto data = line.value().substr(data_prefix.size());
        if (data.starts_with(done_prefix)) {
            // If line starts with "[DONE]", stream ends
            is_parse_done_ = true;
            break;
        }

        try {
            nlohmann::json data_json = nlohmann::json::parse(data);
            if (data_json.contains("choices")) {
                if (auto& choices_json = data_json["choices"];
                    choices_json.is_array() && !choices_json.empty()) {
                    auto& first_choice_json = choices_json[0];
                    auto& delta_json = first_choice_json["delta"];

                    ResponseContent::Choice& choice = response_.choices_.back();
                    if (delta_json.contains("role") &&
                        delta_json["role"].is_string()) {
                        choice.message_.role =
                            delta_json["role"].get<std::string>();
                    }
                    if (delta_json.contains("content") &&
                        delta_json["content"].is_string()) {
                        auto constent_str =
                            delta_json["content"].get<std::string>();
                        if (choice.message_.reasoning_content.has_value() &&
                            !choice.message_.content.has_value()) {
                            out_ << "</thinking>" << '\n';
                        }
                        out_ << constent_str;

                        if (choice.message_.content.has_value()) {
                            choice.message_.content.value() += constent_str;
                        } else {
                            choice.message_.content = constent_str;
                        }
                    }
                    if (delta_json.contains("reasoning_content") &&
                        delta_json["reasoning_content"].is_string()) {
                        auto reasoning_content_str =
                            delta_json["reasoning_content"].get<std::string>();
                        if (!choice.message_.content.has_value() &&
                            !choice.message_.reasoning_content.has_value()) {
                            out_ << "<thinking>" << '\n';
                        }
                        out_ << reasoning_content_str;
                        if (choice.message_.reasoning_content.has_value()) {
                            choice.message_.reasoning_content.value() +=
                                reasoning_content_str;
                        } else {
                            choice.message_.reasoning_content =
                                reasoning_content_str;
                        }
                    }
                    if (delta_json.contains("tool_calls") &&
                        delta_json["tool_calls"].is_array() &&
                        !delta_json["tool_calls"].empty()) {
                        auto const& tool_calls_json = delta_json["tool_calls"];

                        if (tool_calls_json.is_array() &&
                            !tool_calls_json.empty()) {
                            auto const& first_tool_call = tool_calls_json[0];
                            if (first_tool_call.contains("type") &&
                                first_tool_call["type"].is_string() &&
                                first_tool_call["type"].get<std::string>() ==
                                    "function" &&
                                first_tool_call.contains("function") &&
                                first_tool_call["function"].is_object() &&
                                first_tool_call["function"].contains("name")) {
                                choice.message_.tool_calls = tool_calls_json;
                                message_ = delta_json;
                                if (!message_.contains("role")) {
                                    message_["role"] = "assistant";
                                }
                            } else if (choice.message_.tool_calls.has_value() &&
                                       first_tool_call.contains("function") &&
                                       first_tool_call["function"]
                                           .is_object() &&
                                       first_tool_call["function"].contains(
                                           "arguments")) {
                                auto arguments =
                                    choice.message_.tool_calls
                                        .value()[0]["function"]["arguments"]
                                        .get<std::string>() +
                                    first_tool_call["function"]["arguments"]
                                        .get<std::string>();
                                choice.message_.tool_calls
                                    .value()[0]["function"]["arguments"] =
                                    arguments;
                                message_["tool_calls"][0]["function"]
                                        ["arguments"] = arguments;
                            }
                        }
                    }
                    if (first_choice_json.contains("finish_reason") &&
                        first_choice_json["finish_reason"].is_string()) {
                        choice.finish_reason_ =
                            first_choice_json["finish_reason"]
                                .get<std::string>();
                        if (choice.finish_reason_ == "tool_calls") {
                        }
                    }
                    if (first_choice_json.contains("usage")) {
                        auto& usage_json = first_choice_json["usage"];
                        auto get_int_value = [&usage_json](const char* key) {
                            return usage_json[key].is_string()
                                       ? std::stoi(
                                             usage_json[key].get<std::string>())
                                       : usage_json[key].get<int>();
                        };
                        response_.usage_.completion_tokens_ =
                            get_int_value("completion_tokens");
                        response_.usage_.prompt_tokens_ =
                            get_int_value("prompt_tokens");
                        response_.usage_.total_tokens_ =
                            get_int_value("total_tokens");
                    }
                }
            }
            data_jsons_.push_back(data_json);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n```\n"
                      << data << "\n```" << std::endl;
        }
    }
}

bool StreamOperator::parse_done() const { return is_parse_done_; }
