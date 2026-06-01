#include "terminal.h"

#ifdef _WIN32
#include <objbase.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "environment/environment.hpp"
#include "file.h"
#include "subprocess/subprocess.hpp"
#include "temp_file.h"

namespace ai::base {
#if defined(_WIN32)
Terminal::Terminal() {
  in_ = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, 0, nullptr);

  out_ = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                     OPEN_EXISTING, 0, nullptr);
}
Terminal::~Terminal() {
  if (in_ != INVALID_HANDLE_VALUE) {
    CloseHandle(in_);
  }
  if (out_ != INVALID_HANDLE_VALUE) {
    CloseHandle(out_);
  }
}
bool Terminal::available() const {
  return in_ != INVALID_HANDLE_VALUE && out_ != INVALID_HANDLE_VALUE;
}
void Terminal::write(const std::string_view s) const {
  if (INVALID_NATIVE_HANDLE_VALUE == out_) {
    return;
  }
  size_t written{0};
  while (written < s.size()) {
    DWORD w{0};
    BOOL ok = WriteFile(out_, s.data() + written, s.size() - written, &w, 0);
    if (!ok) {
      return;
    }
    written += w;
  }
}
std::string Terminal::read_line() const {
  std::string line;
  if (INVALID_NATIVE_HANDLE_VALUE == in_) {
    return line;
  }
  DWORD read{0};
  char c;
  while (ReadFile(in_, &c, 1, &read, 0)) {
    if (read == 0) {
      break;
    }
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      break;
    }
    line.push_back(c);
  }
  return line;
}
char Terminal::read_char() const {
  if (INVALID_NATIVE_HANDLE_VALUE == in_) {
    return '\0';
  }
  INPUT_RECORD rec;
  DWORD count;

  while (ReadConsoleInputW(in_, &rec, 1, &count)) {
    if (rec.EventType != KEY_EVENT) {
      continue;
    }

    auto& k = rec.Event.KeyEvent;

    if (!k.bKeyDown) {
      continue;
    }

    return static_cast<char>(k.uChar.AsciiChar);
  }

  return '\0';
}

#else
Terminal::Terminal() {
  in_ = ::open("/dev/tty", O_RDONLY);
  out_ = ::open("/dev/tty", O_WRONLY);
}
Terminal::~Terminal() {
  if (in_ != -1) {
    close(in_);
  }
  if (out_ != -1) {
    close(out_);
  }
}
bool Terminal::available() const { return in_ != -1 && out_ != -1; }
void Terminal::write(const std::string_view s) const {
  if (out_ == INVALID_NATIVE_HANDLE_VALUE) {
    return;
  }
  size_t n = 0;
  while (n < s.size()) {
    auto written = ::write(out_, s.data() + n, s.size() - n);
    if (written == -1 && errno == EINTR) {
      continue;
    }
    if (written == -1) {
      break;
    }
    n += written;
  }
}

std::string Terminal::read_line() const {
  std::string line;
  if (in_ == INVALID_NATIVE_HANDLE_VALUE) {
    return line;
  }
  char c;
  while (::read(in_, &c, 1) == 1) {
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      break;
    }
    line.push_back(c);
  }
  return line;
}

char Terminal::read_char() const {
  if (in_ == INVALID_NATIVE_HANDLE_VALUE) {
    return '\0';
  }
  termios oldt{};
  tcgetattr(in_, &oldt);

  termios raw = oldt;
  raw.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(in_, TCSANOW, &raw);

  char c = '\0';

  ::read(in_, &c, 1);

  tcsetattr(in_, TCSANOW, &oldt);

  return c;
}

#endif

bool Terminal::confirm(std::string_view message, bool default_yes) const {
  for (;;) {
    std::string msg;
    msg.reserve(message.size() + 16);
    msg.append("⚠️ ");

    for (auto c : message) {
      if (c == '\n') {
        msg.append("\n   ");
      } else {
        msg.push_back(c);
      }
    }

    write(msg);

    write(default_yes ? " [Y/n] " : " [y/N] ");

    auto line = read_line();

    std::ranges::transform(line, line.begin(),
                           [](unsigned char c) { return std::tolower(c); });

    if (line.empty()) {
      return default_yes;
    }

    if (line == "y" || line == "yes") {
      return true;
    }

    if (line == "n" || line == "no") {
      return false;
    }

    write("Please answer yes or no. :");
  }
}

std::size_t Terminal::menu(std::string_view title,
                           std::vector<std::string> const& items) const {
  if (items.empty()) {
    return 0;
  }
  if (!available()) {
    return 0;
  }
  write(std::string(title));
  write("\n\n");

  for (std::size_t i = 0; i < items.size(); ++i) {
    write(std::to_string(i + 1));
    write(". ");
    write(items[i]);
    write("\n");
  }

  for (;;) {
    write("\nSelection: ");

    auto line = read_line();

    try {
      auto n = static_cast<std::size_t>(std::stoul(line));

      if (n >= 1 && n <= items.size()) {
        return n - 1;
      }
    } catch (...) {
    }
  }
  return 0;
}

std::string Terminal::edit(std::string_view initial_content) {
  // 1. Determine the editor to use
  std::string editor;

#ifdef _WIN32
  if (auto env_editor = env::get("VISUAL"); env_editor.has_value()) {
    editor = env_editor.value();
  } else if (auto env_editor = env::get("EDITOR"); env_editor.has_value()) {
    editor = env_editor.value();
  } else {
    // Try some common Windows editors
    editor = "notepad.exe";  // Default to Notepad
  }
#else
  if (auto env_editor = env::get("VISUAL"); env_editor.has_value()) {
    editor = env_editor.value();
  } else if (auto env_editor = env::get("EDITOR"); env_editor.has_value()) {
    editor = env_editor.value();
  }
  if (editor.empty()) {
    editor = "vi";
  }
#endif
  // 2. Create a temporary file
  TempFile tempfile("prompt.", ".md");
  std::string temp_file_path = tempfile.path();
  write_file(temp_file_path, std::string(initial_content));

  // 3. Open the editor with the temporary file
  using subprocess::named_arguments::shell;
#if defined(_WIN32)
  (void)subprocess::run(shell, editor + " " + tempfile.path());
#else
  (void)subprocess::run(shell, editor + " " + tempfile.path());
#endif

  // 4. Read the content of the temporary file
  auto user_input = tempfile.content();
  if (user_input.has_value()) {
    if (*user_input == initial_content) {
      return "";
    }
    return *user_input;
  }
  return "";
}

}  // namespace ai::base
