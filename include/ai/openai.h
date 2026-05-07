#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ai/response.h"

namespace ai {

struct AiArgs;

class OpenAIClient {
 public:
  explicit OpenAIClient(AiArgs const& args);
  ~OpenAIClient();

  // Disable copy
  OpenAIClient(const OpenAIClient&) = delete;
  OpenAIClient& operator=(const OpenAIClient&) = delete;

  // Enable move
  OpenAIClient(OpenAIClient&&) noexcept;
  OpenAIClient& operator=(OpenAIClient&&) noexcept;

  std::optional<openai::Response> chat(
      const std::string& system_prompt,
      const std::vector<std::string>& user_prompts,
      nlohmann::json& chat_history) const;

  std::vector<std::string> models();

 private:
  class Impl;
  std::unique_ptr<Impl> pimpl;
};

}  // namespace ai
