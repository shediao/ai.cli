#ifndef __AI_CLI_STREAM_H__
#define __AI_CLI_STREAM_H__

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

class StreamOperator {
   public:
    StreamOperator();
    StreamOperator(std::ostream& out);

    void parse(std::string_view chunk);

    bool parse_done() const;
    std::string const& content() const { return content_; }
    std::string const& reasoning_content() const { return reasoning_content_; }
    auto const& data_lines() const { return data_lines_; }
    std::vector<char> const& response_data() const { return response_data_; }

    bool is_debug{false};

   private:
    std::optional<std::string> getLine();

   private:
    std::ostream& out_{std::cout};
    std::vector<char> response_data_{};

    std::string content_;
    std::string reasoning_content_;
    std::vector<std::string> data_lines_{};

    size_t parse_index_{0};
    bool is_in_reasoning_parse_data_{false};
    bool is_parse_done_{false};
};

#endif  // __AI_CLI_STREAM_H__
