#include <cstdio>   // For remove()
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
    if (const char* env_editor = std::getenv("EDITOR")) {
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
