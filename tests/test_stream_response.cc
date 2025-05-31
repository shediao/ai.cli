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
