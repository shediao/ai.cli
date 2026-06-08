#pragma once

#include <sstream>
#include <version>

#if defined(__cpp_lib_source_location) && __cpp_lib_source_location
#include <source_location>
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace ai::base {

using LogSeverity = int;

inline constexpr LogSeverity LOGGING_VERBOSE = -1;
inline constexpr LogSeverity LOGGING_DEBUG = 0;
inline constexpr LogSeverity LOGGING_INFO = 1;
inline constexpr LogSeverity LOGGING_WARNING = 2;
inline constexpr LogSeverity LOGGING_ERROR = 3;
inline constexpr LogSeverity LOGGING_FATAL = 4;
inline constexpr LogSeverity LOGGING_NUM_SEVERITIES = 5;

using LoggingDestination = unsigned int;

enum : unsigned int {
  LOG_NONE = 0,
  LOG_TO_FILE = 1 << 0,
  LOG_TO_STDERR = 1 << 1,
  LOG_TO_ALL = LOG_TO_FILE | LOG_TO_STDERR,
};

#if defined(__cpp_lib_source_location) && __cpp_lib_source_location
#define LOG_STREAM(severity) ::ai::base::LogMessage(severity).stream()
#else
#define LOG_STREAM(severity) \
  ::ai::base::LogMessage(severity, __FILE__, __LINE__).stream()
#endif
#define LAZY_STREAM(stream, condition) \
  !(condition) ? (void)0 : ::ai::base::LogMessageVoidify() & (stream)
#define LOG_IS_ON(severity) (::ai::base::ShouldCreateLogMessage(severity))

#define LOG(severity)                                     \
  LAZY_STREAM(LOG_STREAM(::ai::base::LOGGING_##severity), \
              LOG_IS_ON(::ai::base::LOGGING_##severity))
#define LOG_IF(severity, condition)                       \
  LAZY_STREAM(LOG_STREAM(::ai::base::LOGGING_##severity), \
              LOG_IS_ON(::ai::base::LOGGING_##severity) && (condition))

class LogMessageVoidify {
 public:
  LogMessageVoidify() = default;
  void operator&(std::ostream&) {}
};

class LogMessage {
 public:
#if defined(__cpp_lib_source_location) && __cpp_lib_source_location
  LogMessage(LogSeverity severity, const std::source_location location =
                                       std::source_location::current());
#endif
  LogMessage(LogSeverity severity, const char* filename, int line);

  LogMessage(LogMessage&&) = delete;
  LogMessage& operator=(LogMessage&&) = delete;
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  virtual ~LogMessage();

  std::ostream& stream() { return stream_; }
  std::string BuildCrashString() const;

 protected:
  void Flush();

 private:
  void Init(const char* filename, int line);

  const LogSeverity severity_;
  std::ostringstream stream_;
};

bool ShouldCreateLogMessage(LogSeverity severity);
const char* GetSeverityName(int severity);

void SetLogFilePath(const std::string& path);
void SetLogLevel(LogSeverity severity);

}  // namespace ai::base
