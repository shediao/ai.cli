
#include <curl/curl.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "cli_parser.hpp"
#include "openai_client.hpp"
#include "readline/history.h"   // NOLINT
#include "readline/readline.h"  // NOLINT

void save_to_clipboard(std::string const& text);
std::string load_from_clipboard();

std::optional<std::string> get_stream_context(const std::string_view data,
                                              bool& pre_is_reasoning) {
    try {
        nlohmann::json j = nlohmann::json::parse(data);
        if (j.contains("choices") && j["choices"].is_array() &&
            j["choices"].size() > 0 && j["choices"][0].contains("delta")) {
            auto& delta = j["choices"][0]["delta"];
            if (delta.contains("content") && delta["content"].is_string()) {
                auto content = delta["content"].get<std::string>();
                auto ret =
                    pre_is_reasoning ? ("\n<结束思考>\n" + content) : content;
                pre_is_reasoning = false;
                return ret;
            } else if (delta.contains("reasoning_content") &&
                       delta["reasoning_content"].is_string()) {
                auto content = delta["reasoning_content"].get<std::string>();
                auto ret =
                    pre_is_reasoning ? content : ("\n<开始思考>\n" + content);
                pre_is_reasoning = true;
                return ret;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n```\n"
                  << data << "\n```" << std::endl;
    }
    return std::nullopt;
}

constexpr std::string_view data_prefix = "data: ";
constexpr std::string_view event_prefix = "event: ";
constexpr std::string_view event_error_prefix = "event: error";
constexpr std::string_view done_prefix = "[DONE]";

std::optional<std::string> get_new_line(std::string& unparsed_json) {
    auto begin = std::find_if(unparsed_json.begin(), unparsed_json.end(),
                              [](char c) { return c != '\n'; });
    if (begin == unparsed_json.end()) {
        return std::nullopt;
    }
    auto newline_iter = std::find(begin, unparsed_json.end(), '\n');
    if (newline_iter == unparsed_json.end()) {
        return std::nullopt;
    }
    std::string_view line{begin, newline_iter};
    if (line.starts_with(data_prefix) || line.starts_with(event_prefix) ||
        line.starts_with(": keep-alive")) {
        std::string result = std::string(line);
        unparsed_json.erase(unparsed_json.begin(), newline_iter + 1);
        return result;
    }
    return std::nullopt;
}

void process_stream_output(const std::string& chunk, std::ostream& os,
                           std::string& unparsed_json,
                           std::stringstream& response_stream,
                           bool& pre_is_reasoning) {
    unparsed_json += chunk;
    while (true) {
        auto line = get_new_line(unparsed_json);
        if (!line) {
            break;
        }
        if (line->starts_with(data_prefix)) {
            // If line starts with "data: "
            std::string_view data{line->data() + data_prefix.size(),
                                  line->size() - data_prefix.size()};
            if (data.starts_with(done_prefix)) {
                // If line starts with "[DONE]", stream ends
                break;
            }
            if (auto context = get_stream_context(data, pre_is_reasoning);
                context) {
                os << *context;
                response_stream << *context;
            }
        } else if (line->starts_with(": keep-alive")) {
            continue;
        } else if (line->starts_with(event_error_prefix)) {
            std::cerr << "Error: " << *line << std::endl;
            break;
        } else if (line->starts_with(event_prefix)) {
            // 获取event
            continue;
        } else {
            // 遇到可能是错误信息，直接退出当前处理
            break;
        }
    }
}

std::string get_chat_title(OpenAIClient& client,
                           nlohmann::json const& chat_history) {
    std::string get_tiltle_system_prompt =
        "你是一个AI聊天内容总结助手，请根据用户提供的聊天内容和可选的AI的回复内"
        "容生成一个标题，标题应该简洁明了，不超过25个字。";
    std::string title = client.chat(get_tiltle_system_prompt, "", chat_history);
    return title;
}

void interactive_mode(OpenAIClient& client, const std::string& system_prompt,
                      std::string const& prompt,
                      const OpenAIClient::Config& config) {
    std::cout << "Entering interactive mode. Type 'exit' or 'quit' to exit."
              << std::endl;
    std::cout << "System prompt: "
              << (system_prompt.empty() ? "not set" : system_prompt)
              << std::endl;

    rl_initialize();
    rl_variable_bind("editing-mode", "vi");
    nlohmann::json chat_history = nlohmann::json::array();

    std::vector<std::string> exit_commands = {"exit", "quit", ":q"};
    bool first_line = true;
    while (true) {
        std::string line;
        if (first_line && !prompt.empty()) {
            line = prompt;
            first_line = false;
        } else {
            auto line_ptr = std::unique_ptr<char>(readline("Me> "));
            if (!line_ptr ||
                std::find(exit_commands.begin(), exit_commands.end(),
                          line_ptr.get()) != exit_commands.end()) {
                break;
            }
            line = line_ptr.get();
        }
        if (line.empty()) {
            continue;
        }
        add_history(line.c_str());

        try {
            if (config.stream) {
                setbuf(stdout, nullptr);
                std::cout.rdbuf()->pubsetbuf(nullptr, 0);
                std::string unparsed_json;
                std::stringstream response_stream;
                bool pre_is_reasoning = false;
                client.chat(system_prompt, line, chat_history,
                            [&config, &unparsed_json, &response_stream,
                             &pre_is_reasoning](const std::string& chunk) {
                                if (config.debug) {
                                    std::cout << chunk;
                                    return;
                                }
                                process_stream_output(
                                    chunk, std::cout, unparsed_json,
                                    response_stream, pre_is_reasoning);
                            });
                std::cout << std::endl;
                std::string response = response_stream.str();
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "user"}, {"content", line}}));
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "assistant"}, {"content", response}}));
                save_to_clipboard(response);
            } else {
                std::string response =
                    client.chat(system_prompt, line, chat_history);
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "user"}, {"content", line}}));
                chat_history.push_back(nlohmann::json::object(
                    {{"role", "assistant"}, {"content", response}}));
                save_to_clipboard(response);
                std::cout << "\nAI> " << response << std::endl;
            }
            if (config.debug) {
                std::cout << "Chat history: " << chat_history.dump(2)
                          << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

class CurlGlobalInitGuard {
   public:
    CurlGlobalInitGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInitGuard() { curl_global_cleanup(); }
};

int main(int argc, const char* argv[]) {
    CurlGlobalInitGuard guard;
    try {
        CLIParser parser;
        auto& args = parser.parse(argc, argv);

        if (args.help) {
            parser.print_help();
            return 0;
        }

        if (args.version) {
            parser.print_version();
            return 0;
        }

        if (!args.interactive && args.prompt.empty()) {
            std::cerr << "Error: Must provide a prompt or use interactive mode."
                      << std::endl;
            parser.print_help();
            return 1;
        }

        auto config = parser.to_client_config(args);
        OpenAIClient client(config);

        std::string prompt = args.prompt;
        if (prompt == "-") {
            prompt = std::string{std::istreambuf_iterator<char>(std::cin),
                                 std::istreambuf_iterator<char>()};
        } else if (prompt.starts_with("@")) {
            std::string file_name = prompt.substr(1);
            std::ifstream file(file_name);
            if (!file) {
                std::cerr << "Error: Cannot open file: " << file_name
                          << std::endl;
                return 1;
            }
            prompt = std::string{std::istreambuf_iterator<char>(file),
                                 std::istreambuf_iterator<char>()};
        }

        if (args.interactive) {
            interactive_mode(client, args.system_prompt.value_or(""), prompt,
                             config);
        } else {
            try {
                nlohmann::json chat_history = nlohmann::json::array();
                if (args.stream) {
                    setbuf(stdout, nullptr);
                    std::cout.rdbuf()->pubsetbuf(nullptr, 0);
                    std::string unparsed_json;
                    std::stringstream response_stream;

                    bool pre_is_reasoning = false;
                    client.chat(args.system_prompt.value_or(""), prompt,
                                chat_history,
                                [&unparsed_json, &response_stream, &config,
                                 &pre_is_reasoning](const std::string& chunk) {
                                    if (config.debug) {
                                        std::cout << "DEBUG: " << chunk;
                                        return;
                                    }
                                    process_stream_output(
                                        chunk, std::cout, unparsed_json,
                                        response_stream, pre_is_reasoning);
                                });
                    if (!unparsed_json.empty() &&
                        unparsed_json.find("error") != std::string::npos) {
                        std::cerr << unparsed_json;
                    }
                    std::string response = response_stream.str();
                    std::cout << std::endl;
                    chat_history.push_back(nlohmann::json::object(
                        {{"role", "user"}, {"content", prompt}}));
                    chat_history.push_back(nlohmann::json::object(
                        {{"role", "assistant"}, {"content", response}}));
                    save_to_clipboard(response);
                } else {
                    std::string response = client.chat(
                        args.system_prompt.value_or(""), prompt, chat_history);
                    std::cout << response << std::endl;
                    chat_history.push_back(nlohmann::json::object(
                        {{"role", "user"}, {"content", prompt}}));
                    chat_history.push_back(nlohmann::json::object(
                        {{"role", "assistant"}, {"content", response}}));
                    save_to_clipboard(response);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                return 1;
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
