#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "./args.h"

struct ResponseContent {
    std::string content;
    std::string reasoning_content;
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
