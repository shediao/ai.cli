#include <curl/curl.h>

#include <cstdio>   // For std::remove
#include <cstdlib>  // For system()
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>  // For std::runtime_error
#include <string>

#ifdef _WIN32
#include <windows.h>  // For GetEnvironmentVariable
#else
#include <unistd.h>  // For access
#endif

#include "./args.h"
#include "./utils.h"

TempFile::TempFile(std::string const &prefix, std::string const &postfix)
    : path_{getTempFilePath(prefix, postfix)} {}
TempFile::TempFile() : TempFile("", "") {}
TempFile::~TempFile() {
    if (!path_.empty() && std::filesystem::exists(path_)) {
        std::filesystem::remove(path_);
    }
}
std::string const &TempFile::path() const { return path_; }
std::optional<std::string> TempFile::content() const {
    if (std::ifstream file(path_); file.is_open()) {
        std::string file_content{std::istreambuf_iterator<char>(file),
                                 std::istreambuf_iterator<char>()};
        file.close();
        return file_content;
    }
    return std::nullopt;
}

std::string getTempFilePath(std::string const &prefix,
                            std::string const &postfix) {
    std::string temp_file_path;

#ifdef _WIN32
    char temp_dir[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_dir) == 0) {
        throw std::runtime_error("Failed to get temporary directory.");
    }

    char temp_file[MAX_PATH];
    if (GetTempFileNameA(temp_dir, prefix.c_str(), 0, temp_file) == 0) {
        throw std::runtime_error("Failed to create temporary file.");
    }
    temp_file_path = temp_file;
    if (!postfix.empty()) {
        temp_file_path += postfix;
    }

#else
    std::string template_str = "/tmp/";
    if (!prefix.empty()) {
        template_str += prefix;
    }
    template_str += "XXXXXX";
    if (!postfix.empty()) {
        template_str += postfix;
    }
    int fd = mkstemp(template_str.data());
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file.");
    }
    close(fd);
    temp_file_path = template_str;
#endif
    return temp_file_path;
}

std::string getUserInputViaEditor() {
    // 1. Determine the editor to use
    std::string editor;

#ifdef _WIN32
    char editor_path[MAX_PATH];
    if (GetEnvironmentVariable("EDITOR", editor_path, MAX_PATH) > 0) {
        editor = editor_path;
    } else {
        // Try some common Windows editors
        editor = "notepad.exe";  // Default to Notepad
    }
#else
    if (const char *env_editor = std::getenv("EDITOR")) {
        editor = env_editor;
    } else {
        // Try some common Linux/macOS editors
        if (access("/usr/bin/nano", X_OK) == 0) {
            editor = "/usr/bin/nano";
        } else if (access("/usr/bin/vim", X_OK) == 0) {
            editor = "/usr/bin/vim";
        } else if (access("/usr/bin/vi", X_OK) == 0) {
            editor = "/usr/bin/vi";
        } else {
            // Default to vi or nano if available
            // If not available throw an exception.
            throw std::runtime_error(
                "No suitable editor found.  Please set the EDITOR environment "
                "variable.");
        }
    }
#endif
    // 2. Create a temporary file
    TempFile tempfile("prompt.", ".md");
    std::string temp_file_path = tempfile.path();

    // 3. Open the editor with the temporary file
    std::string command =
        editor + " \"" + tempfile.path() + "\"";  // Wrap path in quotes

    int result = std::system(command.c_str());

    // Handle errors from system call (editor not found, etc.)
    if (result != 0) {
        throw std::runtime_error(
            "Failed to execute editor: " + editor +
            ", system return code: " + std::to_string(result));
    }

    // 4. Read the content of the temporary file
    auto user_input = tempfile.content();

    return user_input.value_or("");
}

// 回调函数，用于处理从 libcurl 接收到的数据
// ptr: 指向接收到的数据块
// size: 每个数据单元的大小（通常是1）
// nmemb: 数据单元的数量
// userdata: 用户自定义指针，这里我们将传递一个 std::ofstream*
static size_t write_data_to_file(void *ptr, size_t size, size_t nmemb,
                                 void *stream) {
    std::ofstream *out_file = static_cast<std::ofstream *>(stream);
    if (out_file && out_file->is_open()) {
        out_file->write(static_cast<char *>(ptr), size * nmemb);
        if (out_file->fail()) {
            // 写入失败，返回0会使libcurl中止传输并返回CURLE_WRITE_ERROR
            return 0;
        }
        return size * nmemb;  // 返回成功写入的字节数
    }
    return 0;  // 如果文件流无效，也中止传输
}

bool download_image(std::string const &image_url, std::string const &image_path,
                    std::string &memi_type) {
    CURL *curl_handle;
    CURLcode res;
    std::ofstream outfile;

    // 1. 初始化 libcurl 全局环境 (通常在程序开始时调用一次)
    // 如果你的程序中多处使用 libcurl，可以考虑将全局初始化/清理放到 main
    // 函数或类构造/析构中 这里为了函数的独立性，每次都调用，但注意
    // curl_global_cleanup 也应对应调用
    // 为简单起见，假设调用者处理全局初始化/清理，或者在main中处理

    // 2. 获取一个 CURL easy handle
    curl_handle = curl_easy_init();
    if (!curl_handle) {
        std::cerr << "Error: curl_easy_init() failed." << std::endl;
        return false;
    }

    // 3. 打开本地文件用于写入 (二进制模式)
    outfile.open(image_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Error: Cannot open file for writing: " << image_path
                  << std::endl;
        curl_easy_cleanup(curl_handle);
        return false;
    }

    // 4. 设置 libcurl 选项
    // 设置要下载的 URL
    curl_easy_setopt(curl_handle, CURLOPT_URL, image_url.c_str());

    // 设置写入数据的回调函数
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_file);

    // 将文件流指针传递给回调函数
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &outfile);

    // 启用 HTTP 3xx 重定向跟随
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    // 在遇到 HTTP 4xx 或 5xx 错误时，让 libcurl 返回错误而不是下载错误页面
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);

    // (可选) 设置超时
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, 10000L);  // 30 秒总超时
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS,
                     3000L);  // 3 秒连接超时

    if (auto &args = AiArgs::instance(); args.proxy.has_value()) {
        curl_easy_setopt(curl_handle, CURLOPT_PROXY,
                         args.proxy.value().c_str());
    }

    // (可选) 详细输出，用于调试
    // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

    // 5. 执行传输
    res = curl_easy_perform(curl_handle);

    // 6. 关闭文件流 (确保所有数据都已刷入磁盘)
    outfile.close();

    // 7. 检查结果
    if (res != CURLE_OK) {
        std::cerr << "Error: curl_easy_perform() failed: "
                  << curl_easy_strerror(res) << std::endl;
        // 如果下载失败，删除可能已创建的不完整文件
        std::remove(image_path.c_str());
        curl_easy_cleanup(curl_handle);
        return false;
    } else {
        // Request was successful, try to get content type
        std::string content_type_str;
        char *ct = nullptr;
        CURLcode info_res =
            curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
        if (info_res == CURLE_OK && ct) {
            content_type_str = ct;
            // The content_type string might have extra info like ";
            // charset=UTF-8" We might want to strip that, e.g., find the first
            // ';'
            auto image_dash_pos = content_type_str.find("image/");
            if (image_dash_pos != std::string::npos) {
                content_type_str = content_type_str.substr(image_dash_pos);
            }
            size_t semi_colon_pos = content_type_str.find(';');
            if (semi_colon_pos != std::string::npos) {
                content_type_str = content_type_str.substr(0, semi_colon_pos);
            }
            memi_type = content_type_str;
        } else {
            std::cerr << "Warning: Could not get content type. "
                      << curl_easy_strerror(info_res) << std::endl;
            // content_type_str will remain empty or you could set a default
        }
    }

    long http_code = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {  // CURLOPT_FAILONERROR
                             // 应该已经处理了大部分，但多一层检查无妨
        std::cerr << "Error: HTTP request failed with code " << http_code
                  << std::endl;
        std::remove(image_path.c_str());
        curl_easy_cleanup(curl_handle);
        return false;
    }

    // 8. 清理 CURL easy handle
    curl_easy_cleanup(curl_handle);

    return true;
}

// Helper function to trim leading/trailing whitespace
static std::string trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

// Callback function to process received headers
// It specifically looks for the Content-Type header
static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
    size_t total_size = size * nitems;
    std::string header_line(buffer, total_size);
    std::string *content_type_ptr = static_cast<std::string *>(userdata);

    // Convert header name to lowercase for case-insensitive comparison
    std::string header_name;
    size_t colon_pos = header_line.find(':');
    if (colon_pos != std::string::npos) {
        header_name = header_line.substr(0, colon_pos);
        std::transform(header_name.begin(), header_name.end(),
                       header_name.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (header_name == "content-type") {
            std::string value = header_line.substr(colon_pos + 1);
            // Trim whitespace and potential extra info (like charset)
            value = trim(value);
            // Sometimes content-type includes charset, e.g., "image/jpeg;
            // charset=utf-8"
            // We only want the MIME type part.
            size_t semi_colon_pos = value.find(';');
            if (semi_colon_pos != std::string::npos) {
                value = value.substr(0, semi_colon_pos);
                value = trim(
                    value);  // Trim again after potentially removing charset
            }
            *content_type_ptr = value;
            // std::cout << "Debug: Found Content-Type: " << *content_type_ptr
            // << std::endl; // Optional debug
        }
    }

    // Must return total_size to indicate all data was processed
    return total_size;
}

/**
 * @brief Gets the MIME type (Content-Type header) of a resource at a URL using
 * libcurl.
 *
 * @param url The URL of the resource (e.g., an image).
 * @return The MIME type string (e.g., "image/jpeg", "image/png") if found,
 *         or an empty string if the Content-Type header is not found or an
 * error occurs.
 */
std::string getMEMI(std::string const &url) {
    CURL *curl = nullptr;
    CURLcode res = CURLE_OK;
    std::string content_type;  // This will store the result

    // Initialize CURL easy handle
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error: curl_easy_init() failed" << std::endl;
        return "";  // Return empty on init failure
    }

    // Set the URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Perform a HEAD request (we only need headers, not the body)
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    // Follow redirects (important for many URLs)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set the header callback function to process headers
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);

    // Pass the address of our content_type string to the callback
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &content_type);

    // Set a timeout (e.g., 10 seconds)
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);  // 30 秒总超时
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                     3000L);  // 3 秒连接超时

    if (auto &args = AiArgs::instance(); args.proxy.has_value()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, args.proxy.value().c_str());
    }

    // For HTTPS: Verify peer and host (recommended for security)
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // Default is 1L
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); // Default is 2L
    // If you have certificate issues in testing, you MIGHT temporarily disable
    // these but this is NOT recommended for production: curl_easy_setopt(curl,
    // CURLOPT_SSL_VERIFYPEER, 0L); curl_easy_setopt(curl,
    // CURLOPT_SSL_VERIFYHOST, 0L);

    // Perform the request
    res = curl_easy_perform(curl);

    // Check for errors during the request
    if (res != CURLE_OK) {
        std::cerr << "Error: curl_easy_perform() failed for URL '" << url
                  << "': " << curl_easy_strerror(res) << std::endl;
        content_type.clear();  // Ensure empty string on error
    } else {
        // Check HTTP response code (optional but good)
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code >= 400) {
            std::cerr << "Warning: HTTP error " << http_code << " for URL '"
                      << url << "'" << std::endl;
            // Depending on requirements, you might want to clear content_type
            // here too content_type.clear();
        }
        // If content_type is still empty after a successful request,
        // it means the server didn't send a Content-Type header.
        if (content_type.empty() && http_code < 400) {
            std::cerr << "Warning: No Content-Type header found for URL '"
                      << url << "'" << std::endl;
        }
    }

    // Cleanup CURL easy handle
    curl_easy_cleanup(curl);

    return content_type;
}
