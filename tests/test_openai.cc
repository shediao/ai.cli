#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "ai/args.h"
#include "ai/openai.h"

using json = nlohmann::json;

// =============================================================================
// Test fixture that provides a default AiArgs instance
// =============================================================================

class OpenAIClientTest : public ::testing::Test {
 protected:
  ai::AiArgs args;
};

class OpenAIClientChatTest : public ::testing::Test {
 protected:
  ai::AiArgs args;
};

class OpenAIClientModelsTest : public ::testing::Test {
 protected:
  ai::AiArgs args;
};

// =============================================================================
// OpenAIClient – Construction & Move Semantics
// =============================================================================

TEST_F(OpenAIClientTest, ConstructDoesNotThrow) {
  EXPECT_NO_THROW({ ai::OpenAIClient client(args); });
}

TEST_F(OpenAIClientTest, MoveConstructor) {
  ai::OpenAIClient src(args);
  // Move-constructing should transfer ownership without issues
  EXPECT_NO_THROW({ ai::OpenAIClient dst(std::move(src)); });
}

TEST_F(OpenAIClientTest, MoveAssignment) {
  ai::OpenAIClient src(args);
  ai::OpenAIClient dst(args);
  // Move-assigning should transfer ownership without issues
  EXPECT_NO_THROW({ dst = std::move(src); });
}

// =============================================================================
// OpenAIClient::chat() – Early-return logic (no network call)
//
// When user_prompts are empty and chat_history is empty (or ends with a
// non-user/non-tool role), chat() returns std::nullopt before attempting
// any CURL/network operation.
// =============================================================================

TEST_F(OpenAIClientChatTest, EmptyPromptsEmptyHistoryReturnsNullopt) {
  ai::OpenAIClient client(args);
  json history = json::array();

  auto result = client.chat(
      /* system_prompt */ "",
      /* user_prompts   */ {},
      /* chat_history   */ history);

  EXPECT_FALSE(result.has_value());
}

TEST_F(OpenAIClientChatTest,
       EmptyPromptsHistoryEndsWithAssistantReturnsNullopt) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back({{"role", "user"}, {"content", "Hello"}});
  history.push_back({{"role", "assistant"}, {"content", "Hi there!"}});

  auto result = client.chat(
      /* system_prompt */ "",
      /* user_prompts   */ {},
      /* chat_history   */ history);

  EXPECT_FALSE(result.has_value());
}

TEST_F(OpenAIClientChatTest, EmptyPromptsHistoryEndsWithSystemReturnsNullopt) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back(
      {{"role", "system"}, {"content", "You are a helpful assistant."}});

  auto result = client.chat(
      /* system_prompt */ "",
      /* user_prompts   */ {},
      /* chat_history   */ history);

  EXPECT_FALSE(result.has_value());
}

TEST_F(OpenAIClientChatTest, EmptyPromptsHistoryOnlySystemReturnsNullopt) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back(
      {{"role", "system"}, {"content", "You are a helpful assistant."}});
  history.push_back({{"role", "user"}, {"content", "Question?"}});
  history.push_back({{"role", "assistant"}, {"content", "Answer."}});

  auto result = client.chat(
      /* system_prompt */ "",
      /* user_prompts   */ {},
      /* chat_history   */ history);

  EXPECT_FALSE(result.has_value());
}

// =============================================================================
// OpenAIClient::chat() – History with valid last role proceeds to network
//
// When user_prompts are empty but chat_history ends with "user" or "tool",
// the method proceeds past the early-return guards and attempts a CURL call.
// Since we are not parsing CLI arguments, chat_args.api_url is empty and
// the CURL call will fail with an exception.
// =============================================================================

TEST_F(OpenAIClientChatTest,
       EmptyPromptsHistoryEndsWithUserThrowsOnNetworkError) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back({{"role", "user"}, {"content", "Hello?"}});

  // The method will attempt to perform a CURL request with an empty URL,
  // which should fail with a runtime_error.
  EXPECT_THROW(
      {
        auto result = client.chat(
            /* system_prompt */ "",
            /* user_prompts   */ {},
            /* chat_history   */ history);
      },
      std::runtime_error);
}

TEST_F(OpenAIClientChatTest,
       EmptyPromptsHistoryEndsWithToolThrowsOnNetworkError) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back({{"role", "user"}, {"content", "Run a command"}});
  history.push_back(
      {{"role", "assistant"},
       {"tool_calls",
        json::array({json::object(
            {{"id", "call_001"},
             {"type", "function"},
             {"function",
              {{"name", "bash"}, {"arguments", "echo hello"}}}})})}});
  history.push_back(
      {{"role", "tool"}, {"tool_call_id", "call_001"}, {"content", "hello"}});

  // Last role is "tool" → proceeds to network → throws
  EXPECT_THROW(
      {
        auto result = client.chat(
            /* system_prompt */ "",
            /* user_prompts   */ {},
            /* chat_history   */ history);
      },
      std::runtime_error);
}

// =============================================================================
// OpenAIClient::chat() – Empty history but valid prompts proceeds to network
// =============================================================================

TEST_F(OpenAIClientChatTest, ValidPromptsEmptyHistoryThrowsOnNetworkError) {
  ai::OpenAIClient client(args);
  json history = json::array();

  EXPECT_THROW(
      {
        auto result = client.chat(
            /* system_prompt */ "",
            /* user_prompts   */ {"What is 1+1?"},
            /* chat_history   */ history);
      },
      std::runtime_error);
}

// =============================================================================
// OpenAIClient::chat() – chat_history is modified in-place (side-effect test)
//
// Even when the network call fails, the method builds the messages array
// and assigns it to chat_history BEFORE the CURL call.  Verify that
// chat_history is updated accordingly even when an exception is thrown later.
// =============================================================================

TEST_F(OpenAIClientChatTest, HistoryIsUpdatedBeforeNetworkCall) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back({{"role", "user"}, {"content", "Previous question"}});
  history.push_back({{"role", "assistant"}, {"content", "Previous answer"}});

  // Adding a new user prompt; the method will:
  // 1. Build messages from history + new prompt
  // 2. Assign it back to chat_history (side effect)
  // 3. Then attempt CURL → throw
  EXPECT_THROW(
      {
        auto result = client.chat(
            /* system_prompt */ "",
            /* user_prompts   */ {"New question"},
            /* chat_history   */ history);
      },
      std::runtime_error);

  // After the call, chat_history should be updated with the new message
  ASSERT_GE(history.size(), 3u);
  EXPECT_EQ(history.back()["role"], "user");
  EXPECT_EQ(history.back()["content"], "New question");
}

TEST_F(OpenAIClientChatTest,
       HistoryWithSystemPromptIsUpdatedBeforeNetworkCall) {
  ai::OpenAIClient client(args);
  json history = json::array();

  EXPECT_THROW(
      {
        auto result = client.chat(
            /* system_prompt */ "You are a math tutor.",
            /* user_prompts   */ {"What is 2+2?"},
            /* chat_history   */ history);
      },
      std::runtime_error);

  // chat_history should contain both the system prompt and the user message
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0]["role"], "system");
  EXPECT_EQ(history[0]["content"], "You are a math tutor.");
  EXPECT_EQ(history[1]["role"], "user");
  EXPECT_EQ(history[1]["content"], "What is 2+2?");
}

TEST_F(OpenAIClientChatTest, HistoryWithExistingSystemPromptIsReplaced) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back(
      {{"role", "system"}, {"content", "You are an old assistant."}});
  history.push_back({{"role", "user"}, {"content", "Hi"}});
  history.push_back({{"role", "assistant"}, {"content", "Hello!"}});

  EXPECT_THROW(
      {
        auto result = client.chat(
            /* system_prompt */ "You are a NEW assistant.",
            /* user_prompts   */ {"Continue"},
            /* chat_history   */ history);
      },
      std::runtime_error);

  // The old system prompt should be removed, new one prepended
  ASSERT_GE(history.size(), 4u);

  // First message should be the new system prompt
  EXPECT_EQ(history[0]["role"], "system");
  EXPECT_EQ(history[0]["content"], "You are a NEW assistant.");

  // There should be exactly one system message
  int system_count = 0;
  for (auto const& msg : history) {
    if (msg.contains("role") && msg["role"] == "system") {
      ++system_count;
    }
  }
  EXPECT_EQ(system_count, 1);
}

TEST_F(OpenAIClientChatTest,
       ReasoningContentPreservedInHistoryWhenNoToolCalls) {
  ai::OpenAIClient client(args);
  json history = json::array();
  history.push_back({{"role", "user"}, {"content", "Why is the sky blue?"}});
  history.push_back(
      {{"role", "assistant"},
       {"content", "The sky is blue because..."},
       {"reasoning_content", "Let me think about Rayleigh scattering..."}});

  EXPECT_THROW(
      {
        auto result = client.chat(
            /* system_prompt */ "",
            /* user_prompts   */ {"Thanks!"},
            /* chat_history   */ history);
      },
      std::runtime_error);

  // reasoning_content is preserved in chat_history because the
  // chat_history = messages assignment happens BEFORE the stripping loop.
  // (The stripping only affects the messages array used for the API request.)
  bool found_reasoning = false;
  for (auto const& msg : history) {
    if (msg.contains("role") && msg["role"] == "assistant" &&
        msg.contains("reasoning_content")) {
      found_reasoning = true;
      EXPECT_EQ(msg["reasoning_content"],
                "Let me think about Rayleigh scattering...");
    }
  }
  EXPECT_TRUE(found_reasoning)
      << "Expected reasoning_content to be preserved in chat_history";
}

// =============================================================================
// OpenAIClient::models() – Network-dependent
// =============================================================================

TEST_F(OpenAIClientModelsTest, ModelsThrowsOnNetworkError) {
  ai::OpenAIClient client(args);

  // models() will attempt a CURL request with an empty URL (since no CLI args
  // were parsed), which should fail with a runtime_error.
  EXPECT_THROW({ auto models = client.models(); }, std::runtime_error);
}
