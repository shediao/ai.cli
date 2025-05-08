#include <curl/curl.h>

#include <cstdio>   // For std::remove
#include <cstdlib>  // For system()
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

#include "./utils.h"

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
    std::string temp_file_path;

#ifdef _WIN32
    char temp_dir[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_dir) == 0) {
        throw std::runtime_error("Failed to get temporary directory.");
    }

    char temp_file[MAX_PATH];
    if (GetTempFileNameA(temp_dir, "edit", 0, temp_file) == 0) {
        throw std::runtime_error("Failed to create temporary file.");
    }
    temp_file_path = temp_file;

#else
    char template_[] = "/tmp/edit.XXXXXX";
    int fd = mkstemp(template_);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file.");
    }
    close(fd);
    temp_file_path = template_;
#endif

    // 3. Open the editor with the temporary file
    std::string command =
        editor + " \"" + temp_file_path + "\"";  // Wrap path in quotes

    int result = std::system(command.c_str());

    // Handle errors from system call (editor not found, etc.)
    if (result != 0) {
        std::remove(temp_file_path.c_str());  // Cleanup if command fails.
        throw std::runtime_error(
            "Failed to execute editor: " + editor +
            ", system return code: " + std::to_string(result));
    }

    // 4. Read the content of the temporary file
    std::string user_input;
    if (std::ifstream file(temp_file_path); file.is_open()) {
        std::copy(std::istreambuf_iterator<char>(file),
                  std::istreambuf_iterator<char>(),
                  std::back_inserter(user_input));
        file.close();
    } else {
        std::remove(
            temp_file_path.c_str());  // Cleanup if file cannot be opened
        throw std::runtime_error("Failed to open temporary file for reading.");
    }

    // 5. Remove the temporary file
    std::remove(temp_file_path.c_str());  // Remove temporary file

    return user_input;
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
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, 30000L);  // 30 秒总超时
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS,
                     10000L);  // 10 秒连接超时

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
