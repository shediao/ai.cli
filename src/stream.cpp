
#include "./stream.h"

#include <iostream>
#include <nlohmann/json.hpp>

constexpr std::string_view data_prefix = "data: ";
constexpr std::string_view done_prefix = "[DONE]";

StreamOperator::StreamOperator(std::ostream& out) : out_{out} {}
StreamOperator::StreamOperator() : StreamOperator(std::cout) {}

std::optional<std::string> get_stream_context(
    const nlohmann::json& j, bool& is_in_reasoning_parse_data) {
    if (j.contains("choices") && j["choices"].is_array() &&
        j["choices"].size() > 0 && j["choices"][0].contains("delta")) {
        auto& delta = j["choices"][0]["delta"];
        if (delta.contains("content") && delta["content"].is_string()) {
            auto content = delta["content"].get<std::string>();
            auto ret = is_in_reasoning_parse_data
                           ? ("\n</thinking>\n" + content)
                           : content;
            is_in_reasoning_parse_data = false;
            return ret;
        } else if (delta.contains("reasoning_content") &&
                   delta["reasoning_content"].is_string()) {
            auto content = delta["reasoning_content"].get<std::string>();
            auto ret = is_in_reasoning_parse_data
                           ? content
                           : ("\n<thinking>\n" + content);
            is_in_reasoning_parse_data = true;
            return ret;
        }
    }
    if (j.contains("choices") && j["choices"].is_array() &&
        j["choices"].size() == 0 && j.contains("usage")) {
        return "\n\n" + j["usage"].dump();
    }
    return std::nullopt;
}

std::optional<std::string> StreamOperator::getLine() {
    auto begin =
        std::find_if(response_data_.begin() + parse_index_,
                     response_data_.end(), [](char c) { return c != '\n'; });
    if (begin == response_data_.end()) {
        return std::nullopt;
    }
    auto newline_iter = std::find(begin, response_data_.end(), '\n');
    if (newline_iter == response_data_.end()) {
        return std::nullopt;
    }

    parse_index_ = newline_iter - response_data_.begin() + 1;
    std::string ret{begin, newline_iter};
    return ret;
}

void StreamOperator::parse(std::string_view chunk) {
    response_data_.insert(response_data_.end(), begin(chunk), end(chunk));
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
        data_lines_.push_back(data);
        if (data.starts_with(done_prefix)) {
            // If line starts with "[DONE]", stream ends
            is_parse_done_ = true;
            break;
        }

        try {
            nlohmann::json data_json = nlohmann::json::parse(data);
            if (auto context =
                    get_stream_context(data_json, is_in_reasoning_parse_data_);
                context) {
                out_ << *context;
                if (is_in_reasoning_parse_data_) {
                    reasoning_content_ += *context;
                } else {
                    content_ += *context;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n```\n"
                      << data << "\n```" << std::endl;
        }
    }
}

bool StreamOperator::parse_done() const { return is_parse_done_; }
