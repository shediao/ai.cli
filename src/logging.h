#ifndef __AI_CLI_LOGGING_H__
#define __AI_CLI_LOGGING_H__
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

inline constexpr LogSeverity LOGGING_VERBOSE = -1;  // This is level 1 verbosity
// Note: the log severities are used to index into the array of names,
// see log_severity_names.
inline constexpr LogSeverity LOGGING_DEBUG = 0;
inline constexpr LogSeverity LOGGING_INFO = 1;
inline constexpr LogSeverity LOGGING_WARNING = 2;
inline constexpr LogSeverity LOGGING_ERROR = 3;
inline constexpr LogSeverity LOGGING_FATAL = 4;
inline constexpr LogSeverity LOGGING_NUM_SEVERITIES = 5;

using LoggingDestination = unsigned int;
// Specifies where logs will be written. Multiple destinations can be specified
// with bitwise OR.
// Unless destination is LOG_NONE, all logs with severity ERROR and above will
// be written to stderr in addition to the specified destination.
// LOG_TO_FILE includes logging to externally-provided file handles.
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
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) {}
};

class LogMessage {
 public:
  LogMessage(const char* file, int line, LogSeverity severity);

  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  virtual ~LogMessage();

  std::ostream& stream() { return stream_; }

  // Gets file:line: message in a format suitable for crash reporting.
  std::string BuildCrashString() const;

 protected:
  void Flush();

 private:
  void Init(const char* file, int line);

  const LogSeverity severity_;
  std::ostringstream stream_;
  size_t message_start_;  // Offset of the start of the message (past prefix
                          // info).
  // The file and line information passed in to the constructor.
  const char* const file_;
  const int line_;
};

bool ShouldCreateLogMessage(LogSeverity severity);
const char* GetSeverityName(int severity);
}  // namespace logging
}  // namespace ai

#endif  // __AI_CLI_LOGGING_H__
