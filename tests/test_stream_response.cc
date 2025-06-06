#include <gtest/gtest.h>

#include <random>

#include "./stream_response_data.h"
#include "response.h"

TEST(StreamResponse, Test1) {
  std::stringstream ss;
  ai::openai::StreamResponse stream_response(ss);
  ai::openai::StreamResponse::parse(response_data.data(), response_data.size(),
                                    1, &stream_response);
  auto non_stream_response = stream_response.toResponse();
  ASSERT_FALSE(non_stream_response.choices().empty());
  std::string s1 =
      (std::string("<thinking>\n") + std::string(reasoning_content) +
       "\n</thinking>\n" + std::string(content));
  std::string s2 = ss.str();
  ASSERT_EQ(s1, s2);
  ASSERT_EQ(non_stream_response.choices().back().finish_reason, "stop");
  ASSERT_EQ(non_stream_response.choices().back().message.content, content);
  ASSERT_EQ(non_stream_response.choices().back().message.reasoning_content,
            reasoning_content);
}

TEST(StreamResponse, Test2) {
  for (int i = 0; i < 20; i++) {
    auto response_str = response_data;
    std::stringstream ss;
    ai::openai::StreamResponse stream_response(ss);

    while (!response_str.empty()) {
      size_t random_number = []() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 100);
        return distrib(gen);
      }();
      if (random_number > response_str.size()) {
        random_number = response_str.size();
      }
      auto data = response_str.substr(0, random_number);
      ai::openai::StreamResponse::parse(data.data(), data.size(), 1,
                                        &stream_response);
      response_str.remove_prefix(random_number);
    }
    std::string s1 =
        (std::string("<thinking>\n") + std::string(reasoning_content) +
         "\n</thinking>\n" + std::string(content));
    std::string s2 = ss.str();
    ASSERT_EQ(s1, s2);
  }
}

TEST(StreamResponse, Test3) {
  std::string_view response_data =
      R"==(data: {"choices":[{"delta":{"content":"Okay, I will help you refine your `README.md`.","role":"assistant"},"index":0}],"created":1749224968,"id":"5w1DaOOlPL2gjrEP3qqSmAY","model":"gemini-2.5-pro-preview-05-06","object":"chat.completion.chunk"}

data: {"choices":[{"delta":{"content":" To do this effectively, I need to analyze the content of both your `README.md` and the core header file `include","role":"assistant"},"index":0}],"created":1749224968,"id":"5w1DaOOlPL2gjrEP3qqSmAY","model":"gemini-2.5-pro-preview-05-06","object":"chat.completion.chunk"}

data: {"choices":[{"delta":{"content":"/argparse/argparse.hpp`.\n\nPlease provide the content of these two files. I'll start by reading","role":"assistant"},"index":0}],"created":1749224968,"id":"5w1DaOOlPL2gjrEP3qqSmAY","model":"gemini-2.5-pro-preview-05-06","object":"chat.completion.chunk"}

data: {"choices":[{"delta":{"content":" them.","role":"assistant"},"index":0}],"created":1749224968,"id":"5w1DaOOlPL2gjrEP3qqSmAY","model":"gemini-2.5-pro-preview-05-06","object":"chat.completion.chunk"}

data: {"choices":[{"delta":{"role":"assistant","tool_calls":[{"function":{"arguments":"{\"paths\":[\"README.md\",\"include/argparse/argparse.hpp\"]}","name":"read_multiple_files"},"id":"","type":"function"}]},"finish_reason":"tool_calls","index":0}],"created":1749224968,"id":"5w1DaOOlPL2gjrEP3qqSmAY","model":"gemini-2.5-pro-preview-05-06","object":"chat.completion.chunk"}

data: [DONE]
  )==";

  auto response_str = response_data;
  std::stringstream ss;
  ai::openai::StreamResponse stream_response(ss);

  while (!response_str.empty()) {
    size_t random_number = []() {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distrib(0, 100);
      return distrib(gen);
    }();
    if (random_number > response_str.size()) {
      random_number = response_str.size();
    }
    auto data = response_str.substr(0, random_number);
    ai::openai::StreamResponse::parse(data.data(), data.size(), 1,
                                      &stream_response);
    response_str.remove_prefix(random_number);
  }
  ASSERT_EQ(
      R"==(Okay, I will help you refine your `README.md`. To do this effectively, I need to analyze the content of both your `README.md` and the core header file `include/argparse/argparse.hpp`.

Please provide the content of these two files. I'll start by reading them.)==",
      ss.str());

  auto response = stream_response.toResponse();
  ASSERT_EQ(response.model(), "gemini-2.5-pro-preview-05-06");
  ASSERT_EQ(response.choices().size(), 1);
  ASSERT_EQ(response.choices().back().finish_reason, "tool_calls");
  ASSERT_EQ(response.choices().back().message.tool_calls.size(), 1);
  ASSERT_EQ(response.choices().back().message.tool_calls[0].function.name,
            "read_multiple_files");
  ASSERT_EQ(response.choices().back().message.tool_calls[0].function.arguments,
            "{\"paths\":[\"README.md\",\"include/argparse/argparse.hpp\"]}");
}
