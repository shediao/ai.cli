#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

class OpenAIClient {
   public:
    struct Config {
        std::string api_key;
        std::string api_url = "https://api.openai.com/v1/chat/completions";
        std::string model = "gpt-3.5-turbo";
        std::string proxy;
        double temperature = 0.1;
        double top_p = 1.0;
        bool stream = false;
        bool debug = false;
        bool verbose = false;
    };

    explicit OpenAIClient(const Config& config);
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
