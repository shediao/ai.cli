#include "openai.h"

#include <curl/curl.h>
#include <curl/easy.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>

#include "args.h"
#include "base64.h"
#include "logging.h"
#include "response.h"
#include "tool_calls.h"
#include "tools/filesystem.h"
#include "utils.h"

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

  std::optional<ai::openai::Response> chat(
      const std::string& system_prompt,
      const std::vector<std::string>& user_prompts,
      nlohmann::json& chat_history) {
    const AiArgs& args_ = AiArgs::instance();
    std::vector<std::string> files;
    std::string user_prompt;
    std::vector<std::string> image_exts{".png", ".jpg", ".jpeg", ".webp",
                                        ".bmp"};
    for (auto const& prompt : user_prompts) {
      if (std::any_of(
              begin(image_exts), end(image_exts),
              [&prompt](auto const& ext) { return prompt.ends_with(ext); })) {
        files.push_back(prompt);
        continue;
      }
      if (prompt.starts_with("https://") || prompt.starts_with("http://")) {
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

    LOG_IF(INFO, !user_prompt.empty()) << "User prompt: " << user_prompt;

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
        return std::nullopt;
      }
      if (auto& last_message = chat_history.back();
          !last_message.contains("role") ||
          !(last_message["role"].get<std::string>() == "user" ||
            last_message["role"].get<std::string>() == "tool")) {
        return std::nullopt;
      }
    }

    nlohmann::json messages = nlohmann::json::array();
    if (!system_prompt.empty()) {
      messages.push_back({{"role", "system"}, {"content", system_prompt}});
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
        messages.push_back({{"role", "user"}, {"content", user_prompt}});
      } else {
        nlohmann::json content = nlohmann::json::array();
        for (auto const& image_url : image_urls) {
          auto kv = nlohmann::json::object();
          kv["url"] = image_url;
          content.push_back({{"type", "image_url"}, {"image_url", kv}});
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
      request["reasoning_effort"] = args_.chat_args.reasoning_effort.value();
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
    LOG(INFO) << "URL: " << url;
    LOG(INFO) << "Request: " << request.dump();

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
      LOG(INFO) << "Proxy: " << args_.proxy.value();
      curl_easy_setopt(curl_, CURLOPT_PROXY, args_.proxy.value().c_str());
    }

    std::string request_body = request.dump();
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_body.c_str());

    ai::openai::StreamResponse stream_response(std::cout);

    if (args_.chat_args.stream) {
#if 0
            curl_easy_setopt(curl_, CURLOPT_BUFFERSIZE, 0L);
            curl_easy_setopt(curl_, CURLOPT_FRESH_CONNECT, 1L);
            curl_easy_setopt(curl_, CURLOPT_FORBID_REUSE, 1L);
#endif
      curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION,
                       ai::openai::StreamResponse::parse);
      curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &stream_response);
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

    auto response = args_.chat_args.stream
                        ? stream_response.toResponse()
                        : ai::openai::Response::from_string(response_string);

    if (!response.choices().empty()) {
      auto& choice = response.choices().front();
      auto message = json::object();
      message["role"] = choice.message.role;
      if (choice.finish_reason == "tool_calls") {
        message["tool_calls"] = choice.message.tool_calls_json();
        chat_history.push_back(message);
      } else {
        if (!choice.message.content.empty()) {
          message["content"] = choice.message.content;
          chat_history.push_back(message);
        }
      }
    }

    return response;
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
      LOG(INFO) << "Proxy: " << args_.proxy.value();
      curl_easy_setopt(curl_, CURLOPT_PROXY, args_.proxy.value().c_str());
    }
    LOG(INFO) << "URL: " << url;

    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_string);

    CURLcode res = curl_easy_perform(curl_);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
      throw std::runtime_error(std::string("CURL error: ") +
                               curl_easy_strerror(res));
    }
    LOG(INFO) << response_string;

    try {
      auto response_json = nlohmann::json::parse(response_string);
      if (response_json.contains("data") && response_json["data"].is_array()) {
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

std::optional<ai::openai::Response> OpenAIClient::chat(
    const std::string& system_prompt,
    const std::vector<std::string>& user_prompts,
    nlohmann::json& chat_history) const {
  return pimpl->chat(system_prompt, user_prompts, chat_history);
}

std::vector<std::string> OpenAIClient::models() { return pimpl->models(); }
