#ifndef __AI_CLI_SRC_RESPONSE_H__
#define __AI_CLI_SRC_RESPONSE_H__
#include <functional>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <vector>
using json = nlohmann::json;

namespace ai {

namespace openai {

class Response {
 public:
  struct Function {
    std::string id;
    std::string name;
    std::string arguments;
  };
  struct Message {
    Message();
    ~Message();
    Message(Message&&);
    Message& operator=(Message&&);
    std::string role;
    std::string content;
    std::string reasoning_content;
    std::vector<Function> tool_calls;
    std::unique_ptr<json> tool_calls_json{nullptr};
  };
  struct Choice {
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
  static Response from_json(json const& response_json);
  inline std::vector<Choice> const& choices() const { return choices_; }
  inline Usage const& usage() const { return usage_; }
  inline std::string const& id() { return id_; }
  inline std::string const& model() { return model_; }

  void add_to_history(json& history);

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
  StreamResponse(StreamResponse&&);
  StreamResponse& operator=(StreamResponse&&);
  ~StreamResponse();

  static size_t parse(const char* ptr, size_t size, size_t nmemb,
                      StreamResponse*);
  Response toResponse();

 private:
  std::unique_ptr<json> all_json_data_{nullptr};
  void parse_impl();
  std::vector<char> response_data_;
  std::size_t parse_index_{0};
  std::reference_wrapper<std::ostream> out_;
};
}  // namespace openai

namespace gemini {}

}  // namespace ai

#endif  // __AI_CLI_SRC_RESPONSE_H__
