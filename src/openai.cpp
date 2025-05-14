#include "./openai.h"

#include <curl/curl.h>
#include <curl/easy.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "./args.h"
#include "./base64.h"
#include "./stream.h"
#include "./tools/filesystem.h"
#include "./tools_call.h"
#include "./utils.h"

namespace {
static std::map<std::string, std::string> memi_map{
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"gif", "image/gif"},
    {"bmp", "image/bmp"},
    {"webp", "image/webp"},
    {"svg", "image/svg+xml"},  // Scalable Vector Graphics
    {"tiff", "image/tiff"},
    {"tif", "image/tiff"},
    {"ico", "image/vnd.microsoft.icon"}  // Or image/x-icon
};
static bool is_image_file(std::string const& f) {
    auto ext_pos = f.find_last_of('.');
    if (ext_pos != std::string::npos) {
        std::string ext = f.substr(ext_pos + 1);
        if (memi_map.find(ext) == memi_map.end()) {
            return false;
        }
        if (std::filesystem::exists(f)) {
            return true;
        }
    }
    return false;
}
static std::string get_image_memi(std::string const& f) {
    auto ext_pos = f.find_last_of('.');
    if (ext_pos != std::string::npos) {
        if (!std::filesystem::exists(f)) {
            return "";
        }
        std::string ext = f.substr(ext_pos + 1);
        if (auto it = memi_map.find(ext); it != memi_map.end()) {
            return it->second;
        }
    }
    return "";
}

}  // namespace

class OpenAIClient::Impl {
   public:
    explicit Impl() {
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
        StreamOperator* stream = static_cast<StreamOperator*>(userdata);
        if (stream) {
            stream->parse({ptr, size * nmemb});
        }
        return size * nmemb;
    }

    ResponseContent chat(const std::string& system_prompt,
                         const std::vector<std::string>& user_prompts,
                         nlohmann::json& chat_history) {
        const AiArgs& args_ = AiArgs::instance();
        std::vector<std::string> files;
        std::string user_prompt;
        std::vector<std::string> image_exts{".png", ".jpg", ".jpeg", ".webp",
                                            ".bmp"};
        if (args_.debug) {
            std::cout << "\n======(history)\n"
                      << chat_history.dump(2) << "\n======\n";
        }
        for (auto const& prompt : user_prompts) {
            if (std::any_of(begin(image_exts), end(image_exts),
                            [&prompt](auto const& ext) {
                                return prompt.ends_with(ext);
                            })) {
                files.push_back(prompt);
                continue;
            }
            if (prompt.starts_with("https://") ||
                prompt.starts_with("http://")) {
                auto memi = getMEMI(prompt);
                if (memi.starts_with("image/")) {
                    files.push_back(prompt);
                    continue;
                }
            }
            if (!user_prompt.empty()) {
                user_prompt += "\n";
            }
            user_prompt += prompt;
        }

        if (args_.debug) {
            if (!files.empty()) {
                for (auto const& f : files) {
                    std::cout << "[](" << f << ")\n";
                }
            }
            std::cout << "User prompt: '" << user_prompt << "'\n";
        }

        auto is_image_url = [](std::string const& url) {
            if (!url.starts_with("https://") && !url.starts_with("http://")) {
                return false;
            }
            return true;
        };
        std::vector<std::string> image_urls;
        for (auto const& f : files) {
            if (is_image_url(f)) {
                std::string memi;
                TempFile img;
                auto download_sucessful = download_image(f, img.path(), memi);
                if (download_sucessful && !memi.empty()) {
                    auto base64 = base64_encode(img.path());
                    image_urls.push_back("data:" + memi + ";base64," + base64);
                } else {
                    std::cerr << "download failed: " << f << '\n';
                }
            } else if (is_image_file(f)) {
                auto memi = get_image_memi(f);
                if (!memi.empty()) {
                    auto base64 = base64_encode(f);
                    image_urls.push_back("data:" + memi + ";base64," + base64);
                }
            }
        }

        if (user_prompt.empty()) {
            // 当user prompt没有的时候判断历史最后一条是否为用户prompt
            if (chat_history.size() == 0) {
                return {};
            }
            if (auto& last_message = chat_history.back();
                !last_message.contains("role") ||
                !(last_message["role"].get<std::string>() == "user" ||
                  last_message["role"].get<std::string>() == "tool")) {
                return {};
            }
        }

        nlohmann::json messages = nlohmann::json::array();
        if (!system_prompt.empty()) {
            messages.push_back(
                {{"role", "system"}, {"content", system_prompt}});
        }
        for (const auto& message : chat_history) {
            if (!system_prompt.empty() && message.contains("role") &&
                "system" == message["role"].get<std::string>()) {
                // 已经提供了system prompt情况下会排除history中的system prompt
                continue;
            }
            messages.push_back(message);
        }
        if (!user_prompt.empty()) {
            if (image_urls.empty()) {
                messages.push_back(
                    {{"role", "user"}, {"content", user_prompt}});
            } else {
                nlohmann::json content = nlohmann::json::array();
                for (auto const& image_url : image_urls) {
                    auto kv = nlohmann::json::object();
                    kv["url"] = image_url;
                    content.push_back(
                        {{"type", "image_url"}, {"image_url", kv}});
                }
                content.push_back({{"type", "text"}, {"text", user_prompt}});
                messages.push_back({{"role", "user"}, {"content", content}});
            }
        }

        nlohmann::json request = {{"model", args_.chat_args.model},
                                  {"messages", messages},
                                  {"stream", args_.chat_args.stream}};

        // 当前对话成为新的历史内容
        chat_history = messages;

        if (args_.chat_args.stream && args_.chat_args.stream_include_usage) {
            nlohmann::json obj;
            obj["include_usage"] = true;
            request["stream_options"] = obj;
        }
        if (args_.chat_args.max_tokens.has_value()) {
            request["max_tokens"] = args_.chat_args.max_tokens.value();
        }

        if (args_.chat_args.temperature.has_value()) {
            request["temperature"] = args_.chat_args.temperature.value();
        }
        if (args_.chat_args.top_p.has_value()) {
            request["top_p"] = args_.chat_args.top_p.value();
        }

        if (args_.chat_args.reasoning_effort.has_value()) {
            request["reasoning_effort"] =
                args_.chat_args.reasoning_effort.value();
        }
        if (args_.chat_args.tools.contains("filesystem")) {
            auto tools = nlohmann::json::parse(get_filesystem_tools());

            if (true) {
                auto tools_for_deepseek = nlohmann::json::array();
                for (auto tool : tools) {
                    auto t = nlohmann::json::object();
                    t["type"] = "function";
                    tool.erase("type");
                    t["function"] = tool;
                    tools_for_deepseek.push_back(t);
                }
                request["tools"] = tools_for_deepseek;
            } else {
                request["tools"] = tools;
            }
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
        if (!args_.api_key.empty()) {
            headers = curl_slist_append(
                headers, ("Authorization: Bearer " + args_.api_key).c_str());
        }
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

        if (args_.proxy.has_value()) {
            if (args_.debug) {
                std::cout << "Proxy: " << args_.proxy.value() << std::endl;
            }
            curl_easy_setopt(curl_, CURLOPT_PROXY, args_.proxy.value().c_str());
        }

        std::string request_body = request.dump();
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_body.c_str());

        std::stringstream eat;
        StreamOperator stream{args_.debug ? eat : std::cout};
        stream.is_debug = args_.debug;
        if (args_.chat_args.stream) {
#if 0
            curl_easy_setopt(curl_, CURLOPT_BUFFERSIZE, 0L);
            curl_easy_setopt(curl_, CURLOPT_FRESH_CONNECT, 1L);
            curl_easy_setopt(curl_, CURLOPT_FORBID_REUSE, 1L);
#endif
            curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION,
                             stream_callback_wrapper);
            curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &stream);
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
            ResponseContent ret;
            ret.response_body_ = response_string;
            if (args_.debug) {
                std::cout << "Response: " << response_string << std::endl;
            }
            try {
                auto response_json = nlohmann::json::parse(response_string);
                if (response_json.contains("choices")) {
                    auto& choices_json = response_json["choices"];
                    if (choices_json.is_array()) {
                        for (auto& choice_json : choices_json) {
                            ResponseContent::Choice choice;
                            if (choice_json.contains("finish_reason") &&
                                choice_json["finish_reason"].is_string()) {
                                choice.finish_reason_ =
                                    choice_json["finish_reason"]
                                        .get<std::string>();
                            }
                            if (choice_json.contains("message")) {
                                auto& message_json = choice_json["message"];
                                if (message_json.contains("content") &&
                                    message_json["content"].is_string()) {
                                    choice.message_.content =
                                        message_json["content"]
                                            .get<std::string>();
                                }
                                if (message_json.contains(
                                        "reasoning_content") &&
                                    message_json["reasoning_content"]
                                        .is_string()) {
                                    choice.message_.reasoning_content =
                                        message_json["reasoning_content"]
                                            .get<std::string>();
                                }
                                if (message_json.contains("role") &&
                                    message_json["role"].is_string()) {
                                    choice.message_.role =
                                        message_json["role"].get<std::string>();
                                }
                                if (message_json.contains("tool_calls") &&
                                    message_json["tool_calls"].is_array()) {
                                    choice.message_.tool_calls =
                                        message_json["tool_calls"];
                                }
                                if (choice.finish_reason_ == "tool_calls") {
                                    chat_history.push_back(message_json);
                                }
                            }
                            ret.choices_.push_back(std::move(choice));
                        }
                    }
                }
                if (response_json.contains("usage")) {
                    auto& usage_json = response_json["usage"];
                    auto get_int_value = [&usage_json](const char* key) {
                        return usage_json[key].is_string()
                                   ? std::stoi(
                                         usage_json[key].get<std::string>())
                                   : usage_json[key].get<int>();
                    };
                    ret.usage_.completion_tokens_ =
                        get_int_value("completion_tokens");
                    ret.usage_.prompt_tokens_ = get_int_value("prompt_tokens");
                    ret.usage_.total_tokens_ = get_int_value("total_tokens");
                }
                return ret;
            } catch (const nlohmann::json::exception& e) {
                std::cerr << e.what() << '\n';
                throw std::runtime_error(response_string);
            }
        } else {
            if (!stream.parse_done()) {
                if (stream.data_jsons().empty()) {
                    try {
                        auto x = nlohmann::json::parse(stream.response_data());
                        // TODO:
                        std::cerr << x.dump() << '\n';
                    } catch (...) {
                        std::cerr
                            << std::string_view{stream.response_data().data(),
                                                stream.response_data().size()}
                            << '\n';
                    }
                }
            }
            auto& ret = stream.response_content();
            if (ret.finish_reason() == "tool_calls") {
                chat_history.push_back(stream.message());
            }
            return ret;
        }

        return {};
    }

    std::vector<std::string> models() {
        const AiArgs& args_ = AiArgs::instance();
        std::string url = args_.models_args.api_url;
        std::string response_string;
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (!args_.api_key.empty()) {
            headers = curl_slist_append(
                headers, ("Authorization: Bearer " + args_.api_key).c_str());
        }
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
            } else {
                std::cout << response_string << '\n';
            }
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error(response_string);
        }
        return {};
    }

   private:
    CURL* curl_;
};

OpenAIClient::OpenAIClient() : pimpl(std::make_unique<Impl>()) {}
OpenAIClient::~OpenAIClient() = default;
OpenAIClient::OpenAIClient(OpenAIClient&&) noexcept = default;
OpenAIClient& OpenAIClient::operator=(OpenAIClient&&) noexcept = default;

ResponseContent OpenAIClient::chat(const std::string& system_prompt,
                                   const std::vector<std::string>& user_prompts,
                                   nlohmann::json& chat_history) const {
    return pimpl->chat(system_prompt, user_prompts, chat_history);
}

std::vector<std::string> OpenAIClient::models() { return pimpl->models(); }
