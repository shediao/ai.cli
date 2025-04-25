#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "args.h"

class OpenAIClient {
   public:
    explicit OpenAIClient(const AiArgs& args);
    ~OpenAIClient();

    // 禁用拷贝
    OpenAIClient(const OpenAIClient&) = delete;
    OpenAIClient& operator=(const OpenAIClient&) = delete;

    // 启用移动
    OpenAIClient(OpenAIClient&&) noexcept;
    OpenAIClient& operator=(OpenAIClient&&) noexcept;

    // 发送聊天请求
    std::string chat(const std::string& system_prompt,
                     const std::string& user_prompt,
                     nlohmann::json const& chat_history,
                     const std::function<void(const std::string&)>&
                         stream_callback = nullptr) const;

   private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};
