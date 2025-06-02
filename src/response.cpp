
#include "response.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>

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
[[maybe_unused]]
bool append_string(std::string const& key, json const& j, std::string& result) {
  if (is_string(key, j)) {
    result += j[key].get<std::string>();
    return true;
  } else {
    return false;
  }
}
[[maybe_unused]]
bool get_string(std::string const& key, json const& j,
                std::optional<std::string>& result) {
  if (is_string(key, j)) {
    result = j[key].get<std::string>();
    return true;
  } else {
    return false;
  }
}
[[maybe_unused]]
bool append_string(std::string const& key, json const& j,
                   std::optional<std::string>& result) {
  if (is_string(key, j)) {
    if (result.has_value()) {
      result.value() += j[key].get<std::string>();
    } else {
      result = j[key].get<std::string>();
    }
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

json Response::Message::tool_calls_json() const {
  auto ret = json::array();
  for (auto const& tool_call : tool_calls) {
    auto t = json::object();
    t["type"] = tool_call.type;
    t["id"] = tool_call.id;
    t["function"] = json::object({{"name", tool_call.function.name},
                                  {"arguments", tool_call.function.arguments}});
    ret.push_back(t);
  }
  return ret;
}

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
        size_t index = choice_json["index"].get<int>();
        if (index + 1 > response.choices_.size()) {
          response.choices_.resize(index + 1);
        }
        Choice& current_choice = response.choices_[index];
        current_choice.index = index;

        get_string("finish_reason", choice_json, current_choice.finish_reason);
        if (is_object("message", choice_json)) {
          auto msg_json = choice_json["message"];
          get_string("role", msg_json, current_choice.message.role);
          get_string("content", msg_json, current_choice.message.content);
          get_string("reasoning_content", msg_json,
                     current_choice.message.reasoning_content);
          if (is_array("tool_calls", msg_json)) {
            for (auto const& tool_call_item : msg_json["tool_calls"]) {
              ToolCall current_tool_call;
              get_string("id", tool_call_item, current_tool_call.id);
              if (is_object("function", tool_call_item)) {
                get_string("name", tool_call_item["function"],
                           current_tool_call.function.name);
                get_string("arguments", tool_call_item["function"],
                           current_tool_call.function.arguments);
                current_choice.message.tool_calls.push_back(current_tool_call);
              }
            }
          }
        }
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

Response Response::from_sse_json(const json& sse_json) {
  Response response;
  for (auto const& chunk : sse_json) {
    if (auto id = chunk["id"].get<std::string>(); response.id_.empty()) {
      response.id_ = id;
    } else if (response.id_ != id) {
      // TODO:
    }
    if (auto model = chunk["model"].get<std::string>();
        response.model_.empty()) {
      response.model_ = model;
    }
    for (auto const& delta : chunk["choices"]) {
      size_t index = delta["index"].get<int>();
      if (index + 1 > response.choices_.size()) {
        response.choices_.resize(index + 1);
        response.choices_[index].index = index;
      }
      auto& choice = response.choices_[index];
      get_string("finish_reason", delta, choice.finish_reason);
      get_string("role", delta["delta"], choice.message.role);

      append_string("content", delta["delta"], choice.message.content);
      append_string("reasoning_content", delta["delta"],
                    choice.message.reasoning_content);
      if (is_array("tool_calls", delta["delta"])) {
        for (auto const& delta_tool_call : delta["delta"]["tool_calls"]) {
          int index{-1};
          get_integer("index", delta_tool_call, index);
          if (index == -1) {
            continue;
          }
          if (index + 1 > static_cast<int>(choice.message.tool_calls.size())) {
            choice.message.tool_calls.resize(index + 1);
          }
          auto& tool_call = choice.message.tool_calls[index];
          get_string("id", delta_tool_call, tool_call.id);
          append_string("name", delta_tool_call["function"],
                        tool_call.function.name);
          append_string("arguments", delta_tool_call["function"],
                        tool_call.function.arguments);
        }
      }
    }
    if (is_object("usage", chunk)) {
      auto const& usage_json = chunk["usage"];
      get_integer("prompt_tokens", usage_json, response.usage_.prompt_tokens);
      get_integer("completion_tokens", usage_json,
                  response.usage_.completion_tokens);
      get_integer("total_tokens", usage_json, response.usage_.total_tokens);
    }
  }

  return response;
}

void Response::add_to_history(json& history) {
  if (!choices_.empty()) {
    json msg = json::object();
    auto arr = json::array();
    msg["role"] = choices_[0].message.role;
    msg["content"] = choices_[0].message.content;
    if (!choices_[0].message.tool_calls.empty()) {
      msg["tool_calls"] = choices_[0].message.tool_calls_json();
    }
    history.push_back(msg);
  }
}

StreamResponse::StreamResponse(std::ostream& out) : out_(out) {}

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
    parse_line(data, all_json_data_, out_);
  }
}

Response StreamResponse::toResponse() {
  return Response::from_sse_json(this->all_json_data_);
}

std::string_view StreamResponse::raw_string() {
  return {response_data_.data(), response_data_.size()};
}

}  // namespace openai
}  // namespace ai
