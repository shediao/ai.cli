
#include "response.h"

#include <nlohmann/json.hpp>

namespace ai {
namespace {
bool is_string(std::string const& key, json const& j) {
  return j.contains(key) && j[key].is_string();
}
bool is_array(std::string const& key, json const& j) {
  return j.contains(key) && j[key].is_array() && !j[key].is_null();
}
bool is_object(std::string const& key, json const& j) {
  return j.contains(key) && j[key].is_object() && !j[key].is_null();
}
bool is_integer(std::string const& key, json const& j) {
  return j.contains(key) && j[key].is_number_integer();
}

bool get_string(std::string const& key, json const& j, std::string& result) {
  if (is_string(key, j)) {
    result = j[key].get<std::string>();
    return true;
  } else {
    return false;
  }
}
bool get_integer(std::string const& key, json const& j, int& result) {
  if (is_integer(key, j)) {
    result = j[key].get<int>();
    return true;
  } else {
    return false;
  }
}
}  // namespace
namespace openai {

Response::Message::~Message() = default;
Response::Message::Message() = default;
Response::Message::Message(Message&&) = default;
Response::Message& Response::Message::operator=(Message&&) = default;
Response Response::from_string(std::string const& response_data) {
  try {
    auto j = json::parse(response_data);
    return from_json(j);
  } catch (json::parse_error const& e) {
    throw std::runtime_error(std::string("JSON parsing error: ") + e.what());
  }
}

Response Response::from_json(json const& response_json) {
  Response response;
  try {
    get_string("id", response_json, response.id_);
    get_string("model", response_json, response.model_);
    if (response_json.contains("usage") && response_json["usage"].is_object()) {
      auto const& usage_json = response_json["usage"];
      get_integer("prompt_tokens", usage_json, response.usage_.prompt_tokens);
      get_integer("completion_tokens", usage_json,
                  response.usage_.completion_tokens);
      get_integer("total_tokens", usage_json, response.usage_.total_tokens);
    }
    if (response_json.contains("choices") &&
        response_json["choices"].is_array()) {
      for (auto const& choice_json : response_json["choices"]) {
        Choice current_choice;
        get_string("finish_reason", choice_json, current_choice.finish_reason);
        if (is_object("message", choice_json)) {
          auto msg_json = choice_json["message"];
          get_string("role", msg_json, current_choice.message.role);
          get_string("content", msg_json, current_choice.message.content);
          get_string("reasoning_content", msg_json,
                     current_choice.message.reasoning_content);
          if (is_array("tool_calls", msg_json)) {
            current_choice.message.tool_calls_json.reset(
                new json(msg_json["tool_calls"]));
            Function current_func;
            for (auto const& tool_call_item : msg_json["tool_calls"]) {
              get_string("id", tool_call_item, current_func.id);
              if (is_object("function", tool_call_item)) {
                get_string("name", tool_call_item["function"],
                           current_func.name);
                get_string("arguments", tool_call_item["function"],
                           current_func.arguments);
                current_choice.message.tool_calls.push_back(current_func);
              }
            }
          }
        }
        response.choices_.push_back(std::move(current_choice));
      }
    }
  } catch (json::parse_error const& e) {
    throw std::runtime_error(std::string("JSON parsing error: ") + e.what());
  } catch (json::type_error const& e) {
    throw std::runtime_error(std::string("JSON type error: ") + e.what());
  } catch (std::exception const& e) {
    throw std::runtime_error(std::string("An unexpected error occurred: ") +
                             e.what());
  }
  return response;
}

void Response::add_to_history(json& history) {
  if (!choices_.empty()) {
    json msg = json::object();
    auto arr = json::array();
    msg["role"] = choices_[0].message.role;
    msg["content"] = choices_[0].message.content;
    if (choices_[0].message.tool_calls_json) {
      msg["tool_calls"] = *choices_[0].message.tool_calls_json;
    }
    history.push_back(msg);
  }
}

StreamResponse::StreamResponse(std::ostream& out) : out_(out) {}
StreamResponse::StreamResponse(StreamResponse&&) = default;
StreamResponse& StreamResponse::operator=(StreamResponse&&) = default;
StreamResponse::~StreamResponse() = default;

size_t StreamResponse::parse(const char* ptr, size_t size, size_t nmemb,
                             StreamResponse* self) {
  self->response_data_.insert(self->response_data_.end(), ptr,
                              ptr + (size * nmemb));
  self->parse_impl();
  return size * nmemb;
}

constexpr std::string_view data_prefix = "data: ";
constexpr std::string_view done_prefix = "[DONE]";

static std::optional<std::string> getLine(std::vector<char>& data,
                                          size_t& index) {
  auto begin = std::find_if(data.begin() + index, data.end(),
                            [](char c) { return c != '\n'; });
  if (begin == data.end()) {
    return std::nullopt;
  }
  auto newline_iter = std::find(begin, data.end(), '\n');
  if (newline_iter == data.end()) {
    return std::nullopt;
  }

  index = newline_iter - data.begin() + 1;
  std::string ret{begin, newline_iter};
  return ret;
}

static void parse_line(std::string const& data, json& all, std::ostream& out) {
  try {
    nlohmann::json const data_json = nlohmann::json::parse(data);
    if (is_array("choices", data_json) && !data_json["choices"].empty()) {
      auto const& choice_json = data_json["choices"][0];
      if (is_object("delta", choice_json)) {
        auto const& delta_json = choice_json["delta"];
        if (is_string("reasoning_content", delta_json)) {
          auto reasoning_content_str =
              delta_json["reasoning_content"].get<std::string>();
          if (all.empty()) {
            out << "<thinking>" << '\n';
          }
          out << reasoning_content_str;
        }
        if (is_string("content", delta_json)) {
          auto constent_str = delta_json["content"].get<std::string>();
          if (!all.empty() && is_string("reasoning_content",
                                        all.back()["choices"][0]["delta"])) {
            out << "\n</thinking>" << '\n';
          }
          out << constent_str;
        }
      }
      all.push_back(data_json);
    }
  } catch (json::parse_error const& e) {
    throw std::runtime_error(std::string("JSON parsing error: ") + e.what() +
                             "`" + data + "`");
  } catch (json::type_error const& e) {
    throw std::runtime_error(std::string("JSON type error: ") + e.what());
  } catch (std::exception const& e) {
    throw std::runtime_error(std::string("An unexpected error occurred: ") +
                             e.what());
  }
}

void StreamResponse::parse_impl() {
  while (true) {
    auto line = getLine(response_data_, parse_index_);
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
      // is_parse_done_ = true;
      break;
    }
    if (!all_json_data_) {
      all_json_data_.reset(new json(json::array()));
    }
    parse_line(data, *all_json_data_, out_);
  }
}

Response StreamResponse::toResponse() {
  auto response = json::object();
  response["choices"] = json::array();
  response["choices"].push_back(json::object());
  auto& choice = response["choices"].back();
  choice["index"] = 0;
  choice["message"] = json::object();
  auto& message = choice["message"];
  for (auto const& data_json : *all_json_data_) {
    if (!is_string("id", response) && is_string("id", data_json)) {
      response["id"] = data_json["id"];
    }
    if (!is_string("model", response) && is_string("model", data_json)) {
      response["model"] = data_json["model"];
    }
    if (is_object("usage", data_json)) {
      response["usage"] = data_json["usage"];
    }
    if (is_array("choices", data_json) && !data_json["choices"].empty()) {
      auto const& choice_json = data_json["choices"][0];
      if (is_object("delta", choice_json)) {
        if (is_string("content", choice_json["delta"])) {
          if (!is_string("content", message)) {
            message["content"] = choice_json["delta"]["content"];
          } else {
            message["content"] =
                message["content"].get<std::string>() +
                choice_json["delta"]["content"].get<std::string>();
          }
        }
        if (is_string("reasoning_content", choice_json["delta"])) {
          if (!is_string("reasoning_content", message)) {
            message["reasoning_content"] =
                choice_json["delta"]["reasoning_content"];
          } else {
            message["reasoning_content"] =
                message["reasoning_content"].get<std::string>() +
                choice_json["delta"]["reasoning_content"].get<std::string>();
          }
        }

        if (!is_string("role", message) &&
            is_string("role", choice_json["delta"])) {
          message["role"] = choice_json["delta"]["role"];
        }
      }
      if (!is_string("finish_reason", choice) &&
          is_string("finish_reason", choice_json)) {
        choice["finish_reason"] = choice_json["finish_reason"];
      }

      if (is_array("tool_calls", choice_json["delta"])) {
        if (!is_array("tool_calls", message)) {
          message["tool_calls"] = choice_json["delta"]["tool_calls"];
        } else {
          auto const& delta_arguments =
              choice_json["delta"]["tool_calls"][0]["function"]["arguments"];
          auto& arguments = message["tool_calls"][0]["function"]["arguments"];
          arguments =
              arguments.get<std::string>() + delta_arguments.get<std::string>();
        }
      }
    }
  }
  return Response::from_json(response);
}

}  // namespace openai
}  // namespace ai
