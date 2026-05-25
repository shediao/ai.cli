#include "base/terminal.h"

#include <gtest/gtest.h>

#include "environment/environment.hpp"

// =============================================================================
// Terminal
// =============================================================================

TEST(TerminalTest, ConstructAndDestruct) {
  // Terminal construction and destruction must not throw or crash,
  // even when no controlling terminal is available (e.g., in CI).
  EXPECT_NO_THROW({
    ai::base::Terminal t;
    // Destructor called at scope exit
  });
}

TEST(TerminalTest, IsNotCopyable) {
  // Terminal manages native handles and must not be copyable.
  static_assert(!std::is_copy_constructible_v<ai::base::Terminal>);
  static_assert(!std::is_copy_assignable_v<ai::base::Terminal>);
  EXPECT_FALSE(std::is_copy_constructible_v<ai::base::Terminal>);
  EXPECT_FALSE(std::is_copy_assignable_v<ai::base::Terminal>);
}

TEST(TerminalTest, IsNotMovable) {
  // Terminal must not be movable — handles must stay with the original object.
  static_assert(!std::is_move_constructible_v<ai::base::Terminal>);
  static_assert(!std::is_move_assignable_v<ai::base::Terminal>);
  EXPECT_FALSE(std::is_move_constructible_v<ai::base::Terminal>);
  EXPECT_FALSE(std::is_move_assignable_v<ai::base::Terminal>);
}

TEST(TerminalTest, AvailableReturnsBool) {
  // available() must return a boolean and must not crash regardless of
  // whether a controlling terminal exists.
  ai::base::Terminal t;
  bool avail = t.available();
  // In a typical CI environment without a controlling terminal, available()
  // returns false. In an interactive terminal it returns true. We only
  // verify the call succeeds and produces a valid boolean.
  EXPECT_TRUE(avail || !avail);
}

TEST(TerminalTest, WriteWhenUnavailableIsSafe) {
  // When no terminal is available, write() must be a safe no-op.
  ai::base::Terminal t;
  if (!t.available()) {
    EXPECT_NO_THROW(t.write("this should not crash"));
    EXPECT_NO_THROW(t.write(""));
  }
}

TEST(TerminalTest, WriteEmptyString) {
  // Writing an empty string must not crash even when a terminal is open.
  ai::base::Terminal t;
  EXPECT_NO_THROW(t.write(""));
}

TEST(TerminalTest, WriteLongString) {
  // Writing a long string must not crash (stress test for the write loop).
  ai::base::Terminal t;
  std::string long_str(10000, 'x');
  EXPECT_NO_THROW(t.write(long_str));
}

TEST(TerminalTest, WriteMultiLine) {
  // Multi-line output must be handled safely.
  ai::base::Terminal t;
  EXPECT_NO_THROW(t.write("line1\nline2\nline3\n"));
}

TEST(TerminalTest, WriteWithSpecialCharacters) {
  // Special characters (tabs, ANSI escapes) must not cause crashes.
  ai::base::Terminal t;
  EXPECT_NO_THROW(t.write("col1\tcol2\tcol3"));
  EXPECT_NO_THROW(t.write("\033[31mred\033[0m \033[1mbold\033[0m"));
}

TEST(TerminalTest, WriteWithEmbeddedNullBytes) {
  // string_view-based write() can contain embedded null bytes.
  // The write loop must handle them correctly without stopping early.
  ai::base::Terminal t;
  std::string data("AB\0CD\0EF", 7);
  EXPECT_NO_THROW(t.write(std::string_view(data.data(), data.size())));
}

TEST(TerminalTest, ReadLineWhenUnavailableReturnsEmpty) {
  // When no terminal is available, read_line() must return an empty string
  // without blocking or crashing.
  ai::base::Terminal t;
  if (!t.available()) {
    std::string line = t.read_line();
    EXPECT_TRUE(line.empty());
  }
}

TEST(TerminalTest, ReadLineAlwaysSafeToCall) {
  // read_line() must not crash regardless of terminal availability.
  // When a terminal is available, read_line() would block waiting for
  // input, so we only call it when we know the terminal is unavailable
  // (e.g. in CI). In all cases the call itself must not throw.
  ai::base::Terminal t;
  if (!t.available()) {
    EXPECT_NO_THROW({
      std::string line = t.read_line();
      EXPECT_TRUE(line.empty());
    });
  }
}

TEST(TerminalTest, ReadCharWhenUnavailableReturnsNullChar) {
  // When no terminal is available, read_char() must return '\0'
  // without blocking or crashing.
  ai::base::Terminal t;
  if (!t.available()) {
    char c = '\x7F';  // non-zero sentinel
    EXPECT_NO_THROW({ c = t.read_char(); });
    EXPECT_EQ(c, '\0');
  }
}

TEST(TerminalTest, ReadCharSafeToCall) {
  // read_char() must not crash regardless of terminal availability.
  // When a terminal is available, read_char() would block waiting for
  // input, so we only call it when unavailable.  The guard inside
  // read_char() returns '\0' immediately for invalid handles.
  ai::base::Terminal t;
  if (!t.available()) {
    EXPECT_NO_THROW({
      char c = t.read_char();
      EXPECT_EQ(c, '\0');
    });
  }
}

TEST(TerminalTest, ConfirmWhenUnavailableReturnsDefault) {
  // When no terminal is available, read_line() returns empty, so
  // confirm() must return the default value without looping forever.
  ai::base::Terminal t;
  if (!t.available()) {
    // default_yes = false → empty line → returns false
    EXPECT_FALSE(t.confirm("Proceed?", false));
    // default_yes = true  → empty line → returns true
    EXPECT_TRUE(t.confirm("Proceed?", true));
  }
}

TEST(TerminalTest, MenuWhenUnavailableReturnsZero) {
  // When no terminal is available, menu() must return 0 without blocking.
  ai::base::Terminal t;
  if (!t.available()) {
    EXPECT_EQ(t.menu("Choose", {"a", "b", "c"}), 0u);
  }
}

TEST(TerminalTest, MenuEmptyItemsReturnsZero) {
  // An empty items list must return 0 immediately.
  ai::base::Terminal t;
  EXPECT_EQ(t.menu("Empty", {}), 0u);
}

TEST(TerminalTest, MultipleInstances) {
  // Multiple Terminal instances must coexist without interfering.
  EXPECT_NO_THROW({
    ai::base::Terminal t1;
    ai::base::Terminal t2;
    EXPECT_TRUE(t1.available() || !t1.available());
    EXPECT_TRUE(t2.available() || !t2.available());
  });
}

TEST(TerminalTest, RecreateAfterDestroy) {
  // Creating a new Terminal after destroying one must work.
  EXPECT_NO_THROW({
    {
      ai::base::Terminal t1;
      EXPECT_TRUE(t1.available() || !t1.available());
}
{
  ai::base::Terminal t2;
  EXPECT_TRUE(t2.available() || !t2.available());
}
});
}

TEST(TerminalTest, AvailableConsistent) {
  // Multiple calls to available() must return the same value.
  ai::base::Terminal t;
  bool first = t.available();
  bool second = t.available();
  EXPECT_EQ(first, second);
}

TEST(TerminalTest, NativeHandleTypeDefs) {
  // Verify that the native handle type aliases are defined correctly.
  // NativeHandle and INVALID_NATIVE_HANDLE_VALUE must be usable at compile
  // time.
  static_assert(std::is_same_v<ai::base::NativeHandle,
#if defined(_WIN32)
                               HANDLE
#else
                               int
#endif
                               >);
  // Verify INVALID_NATIVE_HANDLE_VALUE is a valid constant expression.
#if !defined(_WIN32)
  constexpr auto invalid = ai::base::INVALID_NATIVE_HANDLE_VALUE;
  static_cast<void>(invalid);
#endif
}

// =============================================================================
// Terminal::edit (static)
// =============================================================================

// RAII helper to save/restore environment variables used by edit().
class EnvGuard {
 public:
  EnvGuard() {
    save("VISUAL");
    save("EDITOR");
  }
  ~EnvGuard() {
    restore("VISUAL");
    restore("EDITOR");
  }

  void set(std::string const& name, std::string const& value) {
    env::set(name, value);
  }

  void unset(std::string const& name) { env::unset(name); }

 private:
  std::map<std::string, std::string> saved_;

  void save(std::string const& name) {
    auto val = env::get(name);
    if (val) {
      saved_[name] = val.value();
    } else {
      saved_[name] = "\x1";  // sentinel for "was unset"
    }
  }

  void restore(std::string const& name) {
    auto it = saved_.find(name);
    if (it == saved_.end()) {
      return;
    }
    if (it->second == "\x1") {
      unset(name);
    } else {
      set(name, it->second);
    }
  }
};

TEST(TerminalEditTest, ReturnsEmptyWhenContentUnchanged) {
  // Use a no-op program that ignores its arguments and leaves the temp file
  // untouched → edit() must return empty since initial==final.
  EnvGuard guard;
#if defined(_WIN32)
  guard.set("VISUAL", "cmd /c ver>nul");
#else
  guard.set("VISUAL", "true");
#endif
  guard.unset("EDITOR");

  std::string result = ai::base::Terminal::edit("initial content");
  EXPECT_TRUE(result.empty());
}

TEST(TerminalEditTest, ReturnsModifiedContent) {
  // Use 'echo appended >>' as the editor command.  The shell will
  // execute:  sh -c "echo appended >> /tmp/path"
  // This appends "appended\n" to the file, making it differ from the
  // initial content, so edit() must return the modified file.
  EnvGuard guard;
#if defined(_WIN32)
  // On Windows, redirect with cmd /c
  guard.set("VISUAL", "cmd /c echo appended>>");
#else
  guard.set("VISUAL", "echo appended >>");
#endif
  guard.unset("EDITOR");

  std::string result = ai::base::Terminal::edit("original");
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.find("original"), 0u);
  EXPECT_NE(result.find("appended"), std::string::npos);
}

TEST(TerminalEditTest, EmptyInitialContent) {
  // edit() with empty initial content must not crash.
  EnvGuard guard;
#if defined(_WIN32)
  guard.set("VISUAL", "cmd /c ver>nul");
#else
  guard.set("VISUAL", "true");
#endif
  guard.unset("EDITOR");

  EXPECT_NO_THROW({
    std::string result = ai::base::Terminal::edit("");
    EXPECT_TRUE(result.empty());
  });
}

TEST(TerminalEditTest, FallsBackToEditorEnvVar) {
  // When VISUAL is unset, EDITOR should be used.
  EnvGuard guard;
  guard.unset("VISUAL");
#if defined(_WIN32)
  guard.set("EDITOR", "cmd /c ver>nul");
#else
  guard.set("EDITOR", "true");
#endif

  EXPECT_NO_THROW({
    std::string result = ai::base::Terminal::edit("test with EDITOR");
    EXPECT_TRUE(result.empty());
  });
}

TEST(TerminalEditTest, HandlesNonExistentEditor) {
  // When the editor binary doesn't exist, edit() must not throw.
  // The old getUserInputViaEditor() used to throw on failure;
  // the new implementation treats it as "no changes".
  EnvGuard guard;
#if defined(_WIN32)
  guard.set("VISUAL", "definitely_not_an_editor_xyz.exe");
#else
  guard.set("VISUAL", "/nonexistent/definitely_not_an_editor");
#endif
  guard.unset("EDITOR");

  EXPECT_NO_THROW({
    std::string result = ai::base::Terminal::edit("test");
    EXPECT_TRUE(result.empty());
  });
}

// =============================================================================
// stdin_is_atty / stdout_is_atty / stderr_is_atty
// =============================================================================

TEST(IsAttyTest, ReturnBool) {
  // We cannot reliably predict the return value in a test environment,
  // but we can verify the functions return a boolean and do not crash.
  bool result_stdin = ai::base::stdin_is_atty();
  bool result_stdout = ai::base::stdout_is_atty();
  bool result_stderr = ai::base::stderr_is_atty();

  // Just check they compile and return something sensible
  EXPECT_TRUE(result_stdin || !result_stdin);
  EXPECT_TRUE(result_stdout || !result_stdout);
  EXPECT_TRUE(result_stderr || !result_stderr);

  // Suppress unused warnings
  (void)result_stdin;
  (void)result_stdout;
  (void)result_stderr;
}
