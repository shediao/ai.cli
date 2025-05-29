#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "./args.h"

// {
//   "choices": [
//     {
//       "finish_reason": "tool_calls",
//       "index": 0,
//       "message": {
//         "content": "好的，我将读取 `src/main.cpp` 文件的内容。\n\n",
//         "role": "assistant",
//         "tool_calls": [
//           {
//             "function": {
//               "arguments": "{\"path\":\"src/main.cpp\"}",
//               "name": "read_file"
//             },
//             "id": "",
//             "type": "function"
//           }
//         ]
//       }
//     }
//   ],
//   "created": 1747063800,
//   "model": "gemini-2.5-pro-exp-03-25",
//   "object": "chat.completion",
//   "usage": {
//     "completion_tokens": 36,
//     "prompt_tokens": 270,
//     "total_tokens": 517
//   }
// }

class ResponseContent {
  friend class OpenAIClient;
  friend class StreamOperator;

 public:
  struct Choice {
    struct Message {
      std::string role;
      std::optional<std::string> content;
      std::optional<std::string> reasoning_content;
      std::optional<nlohmann::basic_json<>> tool_calls;
    };
    std::string finish_reason_;
    Message message_;
  };
  struct Usage {
    int completion_tokens_{0};
    int prompt_tokens_{0};
    int total_tokens_{0};
  };

 public:
  std::optional<std::string> content() {
    if (!choices_.empty()) {
      return choices_[0].message_.content;
    }
    return std::nullopt;
  }
  std::optional<std::string> reasoning_content() {
    if (!choices_.empty()) {
      return choices_[0].message_.reasoning_content;
    }
    return std::nullopt;
  }
  std::optional<std::string> finish_reason() {
    if (!choices_.empty()) {
      return choices_[0].finish_reason_;
    }
    return std::nullopt;
  }
  std::optional<nlohmann::basic_json<>> tool_calls() {
    if (!choices_.empty()) {
      return choices_[0].message_.tool_calls;
    }
    return std::nullopt;
  }

 private:
  std::vector<Choice> choices_;
  Usage usage_;
  std::string response_body_;
  std::string response_header_;
};

class OpenAIClient {
 public:
  explicit OpenAIClient();
  ~OpenAIClient();

  // 禁用拷贝
  OpenAIClient(const OpenAIClient&) = delete;
  OpenAIClient& operator=(const OpenAIClient&) = delete;

  // 启用移动
  OpenAIClient(OpenAIClient&&) noexcept;
  OpenAIClient& operator=(OpenAIClient&&) noexcept;

  // 发送聊天请求
  ResponseContent chat(const std::string& system_prompt,
                       const std::vector<std::string>& user_prompts,
                       nlohmann::json& chat_history) const;

  std::vector<std::string> models();

 private:
  class Impl;
  std::unique_ptr<Impl> pimpl;
};
