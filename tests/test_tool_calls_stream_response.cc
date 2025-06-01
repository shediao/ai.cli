
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string_view>

#include "response.h"
constexpr std::string_view response1 =
    R"(data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"role":"assistant","content":""},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"id":"call_0_00213ae3-c4bc-4497-8f41-52bf4f76c3b9","type":"function","function":{"name":"read_file","arguments":""}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\""}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"path"}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"\":"}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"\"."}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"/"}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"test"}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":".txt"}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"tool_calls":[{"index":0,"function":{"arguments":"\"}"}}]},"logprobs":null,"finish_reason":null}]}

data: {"id":"f437f4d1-cf3a-4292-b255-ef88b01ca03f","object":"chat.completion.chunk","created":1748648834,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":""},"logprobs":null,"finish_reason":"tool_calls"}],"usage":{"prompt_tokens":1716,"completion_tokens":21,"total_tokens":1737,"prompt_tokens_details":{"cached_tokens":0},"prompt_cache_hit_tokens":0,"prompt_cache_miss_tokens":1716}}

data: [DONE]
)";

constexpr std::string_view response2 =
    R"(data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"role":"assistant","content":""},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"文件"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" `"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"./"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"test"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":".txt"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"`"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" "},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"中的"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"数学"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"计算"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"是"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" `"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"102"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"4"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"*"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"102"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"4"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"=`"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"。\n\n"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"计算"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"结果是"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"：\n\n"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"\\["},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" "},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"102"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"4"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" \\"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"times"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" "},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"102"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"4"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" ="},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" "},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"1"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":","},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"048"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":","},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"576"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" \\"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"]\n\n"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"所以"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"答案是"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":" **"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"1"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":","},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"048"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":","},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"576"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"**"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":"。"},"logprobs":null,"finish_reason":null}]}

data: {"id":"3426d2d4-6af1-49d3-a0ff-4a3b026b8fdb","object":"chat.completion.chunk","created":1748648839,"model":"deepseek-chat","system_fingerprint":"fp_8802369eaa_prod0425fp8","choices":[{"index":0,"delta":{"content":""},"logprobs":null,"finish_reason":"stop"}],"usage":{"prompt_tokens":1748,"completion_tokens":50,"total_tokens":1798,"prompt_tokens_details":{"cached_tokens":1728},"prompt_cache_hit_tokens":1728,"prompt_cache_miss_tokens":20}}

data: [DONE]
)";

TEST(ToolCallsStreamResponse, Test1) {
  std::stringstream ss;
  ai::openai::StreamResponse stream_response(ss);
  ai::openai::StreamResponse::parse(response1.data(), response1.size(), 1,
                                    &stream_response);
  auto non_stream_response = stream_response.toResponse();
  ASSERT_EQ(non_stream_response.choices().back().finish_reason, "tool_calls");
  ASSERT_EQ(non_stream_response.choices().back().message.content, "");
  ASSERT_EQ(non_stream_response.choices().back().message.reasoning_content, "");
  ASSERT_EQ(non_stream_response.usage().prompt_tokens, 1716);
  ASSERT_EQ(non_stream_response.usage().completion_tokens, 21);
  ASSERT_EQ(non_stream_response.usage().total_tokens, 1737);

  ASSERT_EQ(ss.str(), "");
  ASSERT_FALSE(non_stream_response.choices().back().message.tool_calls.empty());
  ASSERT_EQ(
      non_stream_response.choices().back().message.tool_calls_json().dump(),
      R"==([{"function":{"arguments":"{\"path\":\"./test.txt\"}","name":"read_file"},"id":"call_0_00213ae3-c4bc-4497-8f41-52bf4f76c3b9","type":"function"}])==");
}

TEST(ToolCallsStreamResponse, Test2) {
  std::stringstream ss;
  ai::openai::StreamResponse stream_response(ss);
  ai::openai::StreamResponse::parse(response2.data(), response2.size(), 1,
                                    &stream_response);
  auto non_stream_response = stream_response.toResponse();
  ASSERT_EQ(non_stream_response.choices().back().finish_reason, "stop");
  ASSERT_EQ(non_stream_response.choices().back().message.reasoning_content, "");
  ASSERT_EQ(non_stream_response.usage().prompt_tokens, 1748);
  ASSERT_EQ(non_stream_response.usage().completion_tokens, 50);
  ASSERT_EQ(non_stream_response.usage().total_tokens, 1798);
  std::string_view content =
      R"==(文件 `./test.txt` 中的数学计算是 `1024*1024=`。

计算结果是：

\[ 1024 \times 1024 = 1,048,576 \]

所以答案是 **1,048,576**。)==";
  ASSERT_EQ(non_stream_response.choices().back().message.content, content);
}

TEST(ToolCallsStreamResponse, Test3) {
  std::string_view h1 = R"==([
  {
    "content": "读取文件 ./test.txt 中的内容，其内容为一个数学计算, 请给出这个计算的答案",
    "role": "user"
  }
])==";

  std::string_view h2 = R"==([
  {
    "content": "读取文件 ./test.txt 中的内容，其内容为一个数学计算, 请给出这个计算的答案",
    "role": "user"
  },
  {
    "content": "",
    "role": "assistant",
    "tool_calls": [
      {
        "function": {
          "arguments": "{\"path\":\"./test.txt\"}",
          "name": "read_file"
        },
        "id": "call_0_00213ae3-c4bc-4497-8f41-52bf4f76c3b9",
        "type": "function"
      }
    ]
  }
])==";
  std::string_view h3 = R"==([
  {
    "content": "读取文件 ./test.txt 中的内容，其内容为一个数学计算, 请给出这个计算的答案",
    "role": "user"
  },
  {
    "content": "",
    "role": "assistant",
    "tool_calls": [
      {
        "function": {
          "arguments": "{\"path\":\"./test.txt\"}",
          "name": "read_file"
        },
        "id": "call_0_00213ae3-c4bc-4497-8f41-52bf4f76c3b9",
        "type": "function"
      }
    ]
  },
  {
    "content": "1024*1024=\n",
    "name": "read_file",
    "role": "tool",
    "tool_call_id": "call_0_00213ae3-c4bc-4497-8f41-52bf4f76c3b9"
  },
  {
    "content": "文件 `./test.txt` 中的数学计算是 `1024*1024=`。\n\n计算结果是：\n\n\\[ 1024 \\times 1024 = 1,048,576 \\]\n\n所以答案是 **1,048,576**。",
    "role": "assistant"
  }
])==";

  auto history = json::parse(h1);
  std::stringstream ss1;
  ai::openai::StreamResponse stream_response1(ss1);
  ai::openai::StreamResponse::parse(response1.data(), response1.size(), 1,
                                    &stream_response1);
  auto non_stream_response1 = stream_response1.toResponse();
  non_stream_response1.add_to_history(history);

  ASSERT_EQ(h2, history.dump(2));

  history.push_back(json::parse(R"=(
  {
    "content": "1024*1024=\n",
    "name": "read_file",
    "role": "tool",
    "tool_call_id": "call_0_00213ae3-c4bc-4497-8f41-52bf4f76c3b9"
  }
  )="));

  std::stringstream ss2;
  ai::openai::StreamResponse stream_response2(ss2);
  ai::openai::StreamResponse::parse(response2.data(), response2.size(), 1,
                                    &stream_response2);
  auto non_stream_response2 = stream_response2.toResponse();
  non_stream_response2.add_to_history(history);
  ASSERT_EQ(h3, history.dump(2));
}
