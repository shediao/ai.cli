#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "../src/response.h"
#include "gtest/gtest.h"

TEST(AiCliTest, Response1) {
  auto response = ai::openai::Response::from_string(R"(
{
    "id": "cmpl-04ea926191a14749b7f2c7a48a68abc6",
    "object": "chat.completion",
    "created": 1698999496,
    "model": "moonshot-v1-8k",
    "choices": [
        {
            "index": 0,
            "message": {
                "role": "assistant",
                "content": "你好，李雷！1+1等于2。如果你有其他问题，请随时提问！"
            },
            "finish_reason": "stop"
        }
    ],
    "usage": {
        "prompt_tokens": 19,
        "completion_tokens": 21,
        "total_tokens": 40
    }
}
)");
  ASSERT_EQ(response.id(), "cmpl-04ea926191a14749b7f2c7a48a68abc6");
  ASSERT_EQ(response.model(), "moonshot-v1-8k");
  ASSERT_EQ(response.choices().size(), 1);
  auto& choice = response.choices()[0];
  ASSERT_EQ(choice.finish_reason, "stop");
  ASSERT_EQ(choice.message.content,
            "你好，李雷！1+1等于2。如果你有其他问题，请随时提问！");
  ASSERT_EQ(choice.message.role, "assistant");
  ASSERT_TRUE(choice.message.tool_calls.empty());
  ASSERT_FALSE(choice.message.tool_calls_json);
}

TEST(AiCliTest, Response2) {
  auto response = ai::openai::Response::from_string(R"(
{
    "id": "cmpl-04ea926191a14749b7f2c7a48a68abc6",
    "object": "chat.completion",
    "created": 1698999496,
    "model": "moonshot-v1-8k",
    "choices": [
        {
            "index": 0,
            "finish_reason": "tool_calls",
            "message": {
                "content": "",
                "role": "assistant",
                "tool_calls": [
                    {
                        "id": "search:0",
                        "function": {
                            "arguments": "{\n    \"query\": \"Context Caching\"\n}",
                            "name": "search"
                        },
                        "type": "function"
                    }
                ]
            }
        }
    ],
    "usage": {
        "prompt_tokens": 19,
        "completion_tokens": 21,
        "total_tokens": 40
    }
}
)");

  ASSERT_EQ(response.choices().size(), 1);
  auto& choice = response.choices()[0];
  ASSERT_EQ(choice.finish_reason, "tool_calls");
  ASSERT_EQ(choice.message.content, "");
  ASSERT_EQ(choice.message.role, "assistant");
  ASSERT_EQ(choice.message.tool_calls.size(), 1);
  ASSERT_TRUE(choice.message.tool_calls_json);
  auto tool_call = choice.message.tool_calls[0];
  ASSERT_EQ(tool_call.name, "search");
  ASSERT_EQ(tool_call.id, "search:0");
  ASSERT_EQ(tool_call.arguments, "{\n    \"query\": \"Context Caching\"\n}");

  auto& j = *choice.message.tool_calls_json;
  ASSERT_TRUE(j.is_array());
  ASSERT_EQ(j.size(), 1);
  ASSERT_EQ(j[0]["id"].get<std::string>(), "search:0");
  ASSERT_EQ(j[0]["function"]["name"].get<std::string>(), "search");
  ASSERT_EQ(j[0]["function"]["arguments"].get<std::string>(),
            "{\n    \"query\": \"Context Caching\"\n}");
}
TEST(AiCliTest, History1) {
  json history = json::array();
  json msg1 = json::object();
  msg1["role"] = "user";
  msg1["content"] = "1+1=?";
  history.push_back(msg1);
  auto response = ai::openai::Response::from_string(R"(
{
    "id": "cmpl-04ea926191a14749b7f2c7a48a68abc6",
    "object": "chat.completion",
    "created": 1698999496,
    "model": "moonshot-v1-8k",
    "choices": [
        {
            "index": 0,
            "message": {
                "role": "assistant",
                "content": "3"
            },
            "finish_reason": "stop"
        }
    ],
    "usage": {
        "prompt_tokens": 19,
        "completion_tokens": 21,
        "total_tokens": 40
    }
}
)");
  response.add_to_history(history);

  ASSERT_EQ(history.size(), 2);
  ASSERT_EQ(
      history.dump(),
      R"([{"content":"1+1=?","role":"user"},{"content":"3","role":"assistant"}])");
}
