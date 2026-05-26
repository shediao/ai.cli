#pragma once
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "io.h"

namespace ai::base {
class Terminal {
 public:
  Terminal();
  ~Terminal();
  Terminal(Terminal const&) = delete;
  Terminal(Terminal&&) = delete;
  Terminal& operator=(Terminal const&) = delete;
  Terminal& operator=(Terminal&&) = delete;

  bool available() const;
  void write(const std::string_view s);

  std::string read_line();
  char read_char();

  bool confirm(std::string_view message, bool default_yes = false);

  std::size_t menu(std::string_view title,
                   std::vector<std::string> const& items);

  static std::string edit(std::string_view initial_content = "");

 private:
  NativeHandle in_{INVALID_NATIVE_HANDLE_VALUE};
  NativeHandle out_{INVALID_NATIVE_HANDLE_VALUE};
};

}  // namespace ai::base
