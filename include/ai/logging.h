#pragma once

#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace ai {
namespace logging {

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

#define LOG_STREAM(severity) \
  ::ai::logging::LogMessage(__FILE__, __LINE__, severity).stream()
#define LAZY_STREAM(stream, condition) \
  !(condition) ? (void)0 : ::ai::logging::LogMessageVoidify() & (stream)
#define LOG_IS_ON(severity) (::ai::logging::ShouldCreateLogMessage(severity))

#define LOG(severity)                                        \
  LAZY_STREAM(LOG_STREAM(::ai::logging::LOGGING_##severity), \
              LOG_IS_ON(::ai::logging::LOGGING_##severity))
#define LOG_IF(severity, condition)                          \
  LAZY_STREAM(LOG_STREAM(::ai::logging::LOGGING_##severity), \
              LOG_IS_ON(::ai::logging::LOGGING_##severity) && (condition))

class LogMessageVoidify {
 public:
  LogMessageVoidify() = default;
  void operator&(std::ostream&) {}
};

class LogMessage {
 public:
  LogMessage(const char* file, int line, LogSeverity severity);

  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  virtual ~LogMessage();

  std::ostream& stream() { return stream_; }
  std::string BuildCrashString() const;

 protected:
  void Flush();

 private:
  void Init(const char* file, int line);

  const LogSeverity severity_;
  std::ostringstream stream_;
  size_t message_start_;
  const char* const file_;
  const int line_;
};

bool ShouldCreateLogMessage(LogSeverity severity);
const char* GetSeverityName(int severity);

}  // namespace logging
}  // namespace ai
