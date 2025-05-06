#include "./openai.h"

#include <curl/curl.h>
#include <curl/easy.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "./args.h"

class OpenAIClient::Impl {
   public:
    explicit Impl(const AiArgs& args) : args_(args) {
        curl_ = curl_easy_init();
        if (!curl_) {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }

    ~Impl() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
    }

    static size_t write_callback(char* ptr, size_t size, size_t nmemb,
                                 void* userdata) {
        auto* response = static_cast<std::string*>(userdata);
        response->append(ptr, size * nmemb);
        return size * nmemb;
    }

    static size_t stream_callback_wrapper(char* ptr, size_t size, size_t nmemb,
                                          void* userdata) {
        if (size * nmemb == 0) {
            return 0;
        }
        auto* callback =
            static_cast<std::function<void(const std::string&)>*>(userdata);
        if (callback && *callback) {
            std::string chunk(ptr, size * nmemb);
            (*callback)(chunk);
        }
        return size * nmemb;
    }

    ResponseContent chat(
        const std::string& system_prompt, const std::string& user_prompt,
        nlohmann::json const& chat_history,
        const std::function<void(const std::string&)>& stream_callback) {
        if (args_.debug) {
            std::cout << "System prompt: " << system_prompt << std::endl;
            std::cout << "User prompt: " << user_prompt << std::endl;
        }
        if (system_prompt.empty() && user_prompt.empty()) {
            return {};
        }

        nlohmann::json messages = nlohmann::json::array();
        if (!system_prompt.empty()) {
            messages.push_back(
                {{"role", "system"}, {"content", system_prompt}});
        }
        for (const auto& message : chat_history) {
            messages.push_back(message);
        }
        if (!user_prompt.empty()) {
            messages.push_back({{"role", "user"}, {"content", user_prompt}});
        }

        nlohmann::json request = {{"model", args_.chat_args.model},
                                  {"messages", messages},
                                  {"stream", args_.chat_args.stream}};

        if (args_.chat_args.temperature.has_value()) {
            request["temperature"] = args_.chat_args.temperature.value();
        }
        if (args_.chat_args.top_p.has_value()) {
            request["top_p"] = args_.chat_args.top_p.value();
        }

        std::string url = args_.chat_args.api_url;
        std::string response_string;
        if (args_.debug) {
            std::cout << "URL: " << url << std::endl;
            std::cout << "Request: " << request.dump(2) << std::endl;
        }

        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(
            headers,
            ("Authorization: Bearer " + args_.chat_args.api_key).c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

        if (args_.proxy.has_value()) {
            if (args_.debug) {
                std::cout << "Proxy: " << args_.proxy.value() << std::endl;
            }
            curl_easy_setopt(curl_, CURLOPT_PROXY, args_.proxy.value().c_str());
        }

        std::string request_body = request.dump();
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_body.c_str());

        if (args_.chat_args.stream && stream_callback) {
#if 0
            curl_easy_setopt(curl_, CURLOPT_BUFFERSIZE, 0L);
            curl_easy_setopt(curl_, CURLOPT_FRESH_CONNECT, 1L);
            curl_easy_setopt(curl_, CURLOPT_FORBID_REUSE, 1L);
#endif
            curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION,
                             stream_callback_wrapper);
            curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &stream_callback);
        } else {
            curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);
        }

        CURLcode res = curl_easy_perform(curl_);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("CURL error: ") +
                                     curl_easy_strerror(res));
        }

        if (!args_.chat_args.stream) {
            if (args_.debug) {
                std::cout << "Response: " << response_string << std::endl;
            }
            try {
                auto response_json = nlohmann::json::parse(response_string);
                if (response_json.contains("choices") &&
                    response_json["choices"].is_array() &&
                    response_json["choices"].size() > 0 &&
                    response_json["choices"][0].contains("message") &&
                    response_json["choices"][0]["message"].contains(
                        "content")) {
                    ResponseContent ret;
                    ret.content =
                        response_json["choices"][0]["message"]["content"]
                            .get<std::string>();
                    if (response_json["choices"][0]["message"].contains(
                            "reasoning_content")) {
                        ret.reasoning_content =
                            response_json["choices"][0]["message"]
                                         ["reasoning_content"]
                                             .get<std::string>();
                        return ret;
                    }
                    return ret;
                } else {
                    auto err = response_json.dump(2);
                    throw std::runtime_error(err);
                }
            } catch (const nlohmann::json::exception& e) {
                throw std::runtime_error(std::string("JSON parse error: ") +
                                         e.what());
            }
        } else {
            // stream
        }

        return {};
    }

    std::vector<std::string> models() {
        std::string url = args_.models_args.api_url;
        std::string response_string;
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(
            headers,
            ("Authorization: Bearer " + args_.models_args.api_key).c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

        if (args_.proxy.has_value()) {
            if (args_.debug) {
                std::cout << "Proxy: " << args_.proxy.value() << std::endl;
            }
            curl_easy_setopt(curl_, CURLOPT_PROXY, args_.proxy.value().c_str());
        }
        if (args_.debug) {
            std::cout << "URL: " << url << std::endl;
        }

        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);

        CURLcode res = curl_easy_perform(curl_);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("CURL error: ") +
                                     curl_easy_strerror(res));
        }
        if (args_.debug) {
            std::cout << "repsonse: " << response_string << std::endl;
        }

        try {
            auto response_json = nlohmann::json::parse(response_string);
            if (response_json.contains("data") &&
                response_json["data"].is_array()) {
                std::vector<std::string> models;
                for (auto obj : response_json["data"]) {
                    if (obj.contains("id") && obj["id"].is_string()) {
                        models.push_back(obj["id"]);
                    }
                }
                return models;
            }
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error(std::string("JSON parse error: ") +
                                     e.what());
        }
        return {};
    }

   private:
    const AiArgs& args_;
    CURL* curl_;
};

OpenAIClient::OpenAIClient(const AiArgs& args)
    : pimpl(std::make_unique<Impl>(args)) {}
OpenAIClient::~OpenAIClient() = default;
OpenAIClient::OpenAIClient(OpenAIClient&&) noexcept = default;
OpenAIClient& OpenAIClient::operator=(OpenAIClient&&) noexcept = default;

ResponseContent OpenAIClient::chat(
    const std::string& system_prompt, const std::string& user_prompt,
    nlohmann::json const& chat_history,
    const std::function<void(const std::string&)>& stream_callback) const {
    return pimpl->chat(system_prompt, user_prompt, chat_history,
                       stream_callback);
}

std::vector<std::string> OpenAIClient::models() { return pimpl->models(); }
