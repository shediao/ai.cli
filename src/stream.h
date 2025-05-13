#ifndef __AI_CLI_STREAM_H__
#define __AI_CLI_STREAM_H__

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "./openai.h"
#include "nlohmann/json.hpp"

class StreamOperator {
   public:
    StreamOperator();
    StreamOperator(std::ostream& out);

    void parse(std::string_view chunk);

    bool parse_done() const;
    std::string content() const {
        return response_.choices_[0].message_.content.value_or("");
    }
    std::string reasoning_content() const {
        return response_.choices_[0].message_.reasoning_content.value_or("");
    }
    auto const& data_jsons() const { return data_jsons_; }
    std::string const& response_data() const {
        return response_.response_body_;
    }

    ResponseContent& response_content() {
      return response_;
    }

    bool is_debug{false};

   private:
    std::optional<std::string> getLine();

   private:
    std::ostream& out_{std::cout};

    std::vector<nlohmann::json> data_jsons_{};

    ResponseContent response_;

    size_t parse_index_{0};
    bool is_parse_done_{false};
};

#endif  // __AI_CLI_STREAM_H__
