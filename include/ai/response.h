#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <vector>

namespace ai {
namespace openai {

struct ChatCompletionSnapshot {
  struct Choice {
    struct Message {
      struct ToolCall {
        struct Function {
          std::string arguments_;
          std::string name_;
        };
        std::string id_;
        Function function_;
        std::string type{"function"};
      };
      std::optional<std::string> content{std::nullopt};
      std::optional<std::string> reasoning_content{std::nullopt};
      std::vector<ToolCall> tool_calls_;
      std::string role_;
    };
    Message message_;
    std::optional<std::string> finish_reason_{std::nullopt};
    int index_;
  };
  std::string id_;
  std::vector<Choice> choices_;
  unsigned long long created_;
  std::string model_;
  std::optional<std::string> system_fingerprint_{std::nullopt};
};

class Response {
 public:
  struct Function {
    std::string arguments;
    std::string name;
  };
  struct ToolCall {
    std::string id;
    std::string type{"function"};
    Function function;
  };
  struct Message {
    std::string role;
    std::string content;
    std::optional<std::string> reasoning_content{std::nullopt};
    std::vector<ToolCall> tool_calls;
    nlohmann::json tool_calls_json() const;
  };
  struct Choice {
    int index{-1};
    Message message;
    std::string finish_reason;
  };
  struct Usage {
    int prompt_tokens{0};
    int completion_tokens{0};
    int total_tokens{0};
  };

  Response(Response&&) = default;
  Response& operator=(Response&&) = default;
  Response(Response const&) = default;
  Response& operator=(Response const&) = default;
  ~Response() = default;

  static Response from_string(std::string const& response_data);
  static Response from_json(nlohmann::json const& response_json);
  static Response from_sse_json(const std::vector<nlohmann::json>& sse_json);

  inline std::vector<Choice> const& choices() const { return choices_; }
  inline Usage const& usage() const { return usage_; }
  inline std::string const& id() { return id_; }
  inline std::string const& model() { return model_; }

  void add_to_history(nlohmann::json& history);

 private:
  Response() = default;
  std::string id_;
  std::string model_;
  std::vector<Choice> choices_;
  Usage usage_;
};

class StreamResponse {
 public:
  StreamResponse(std::ostream& out);

  static size_t parse(const char* ptr, size_t size, size_t nmemb,
                      StreamResponse*);
  Response toResponse();
  std::string_view raw_string();

 private:
  std::vector<nlohmann::json> all_json_data_;
  void parse_impl();
  std::vector<char> response_data_;
  std::size_t parse_index_{0};
  std::reference_wrapper<std::ostream> out_;
};

}  // namespace openai
}  // namespace ai
