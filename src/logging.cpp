
#include "logging.h"

#include "args.h"
#if defined(_WIN32)
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ai {
namespace logging {
const char* const log_severity_names[] = {"DEBUG", "INFO", "WARNING", "ERROR",
                                          "FATAL"};
static_assert(LOGGING_NUM_SEVERITIES == std::size(log_severity_names),
              "Incorrect number of log_severity_names");

FILE* g_log_file = nullptr;

const char* GetSeverityName(int severity) {
  if (severity >= 0 && severity < LOGGING_NUM_SEVERITIES) {
    return log_severity_names[severity];
  }
  return "UNKNOWN";
}
std::string GetLogFilePath() { return AiArgs::instance().log_file; }

LoggingDestination GetLogDestination() {
  static LoggingDestination g_logging_destination = []() {
    auto& log_type = AiArgs::instance().log_type;
    if (log_type == "stderr") {
      return LOG_TO_STDERR;
    } else if (log_type == "file") {
      return LOG_TO_FILE;
    }
    return LOG_TO_ALL;
  }();
  return g_logging_destination;
}

#if defined(_WIN32)
using NativeHandle = HANDLE;
const static inline NativeHandle INVALID_NATIVE_HANDLE_VALUE =
    INVALID_HANDLE_VALUE;
#else   // _WIN32
using NativeHandle = int;
constexpr NativeHandle INVALID_NATIVE_HANDLE_VALUE = -1;
#endif  // !_WIN32

inline void WriteToNativeHandle(NativeHandle fd, char* data, size_t len) {
  std::string_view write_view{data, data + len};
  while (!write_view.empty()) {
#if defined(_WIN32)
    DWORD write_count{0};
    if (!WriteFile(fd, write_view.data(), static_cast<DWORD>(write_view.size()),
                   &write_count, 0)) {
      throw std::runtime_error("WriteFile error: " + get_last_error_msg());
    }
    if (write_count > 0) {
      write_view.remove_prefix(static_cast<size_t>(write_count));
    }
#else
    auto write_count = write(fd, write_view.data(), write_view.size());
    if (write_count > 0) {
      write_view.remove_prefix(static_cast<size_t>(write_count));
    }
    if (write_count == -1) {
      throw std::runtime_error("write error: " + std::to_string(errno));
    }
#endif
  }
}

void CloseNativeHandle(NativeHandle& handle) {
  if (handle != INVALID_NATIVE_HANDLE_VALUE) {
#if defined(_WIN32)
    CloseHandle(handle);
#else
    close(handle);
#endif
    handle = INVALID_NATIVE_HANDLE_VALUE;
  }
}
NativeHandle OpenNativeFile(std::string const& path) {
#if defined(_WIN32)
  return CreateFileA(path_.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
#else
  return open(path.c_str(), (O_WRONLY | O_CREAT | O_APPEND), 0644);
#endif
}

NativeHandle GetLogFileNativeHandle() {
  static auto fd = OpenNativeFile(GetLogFilePath());
  return fd;
}

LogMessage::~LogMessage() { Flush(); }
LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_{severity}, file_(file), line_(line) {
  Init(file, line);
}

void LogMessage::Init(const char* file, int line) {
  std::string_view filename = file;

  auto path_pos = filename.rfind('/');
  if (path_pos == std::string_view::npos) {
    path_pos = filename.rfind('\\');
  }

  if (path_pos != std::string_view::npos) {
    filename = filename.substr(path_pos + 1);
  }

  char* g_log_prefix = nullptr;
  bool g_log_process_id = false;
  bool g_log_thread_id = false;
  bool g_log_timestamp = true;
  bool g_log_tickcount = false;

  stream_ << '[';
  if (g_log_prefix) {
    stream_ << g_log_prefix << ':';
  }
  if (g_log_process_id) {
    // stream_ << base::GetUniqueIdForProcess() << ':';
  }
  if (g_log_thread_id) {
    // stream_ << base::PlatformThread::CurrentId() << ':';
  }
  if (g_log_timestamp) {
#if defined(_WIN32)
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);
    stream_ << std::setfill('0') << std::setw(2) << local_time.wMonth
            << std::setw(2) << local_time.wDay << '/' << std::setw(2)
            << local_time.wHour << std::setw(2) << local_time.wMinute
            << std::setw(2) << local_time.wSecond << '.' << std::setw(3)
            << local_time.wMilliseconds << ':';
#else
    timeval tv;
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm local_time;
    localtime_r(&t, &local_time);
    struct tm* tm_time = &local_time;
    stream_ << std::setfill('0') << std::setw(2) << 1 + tm_time->tm_mon
            << std::setw(2) << tm_time->tm_mday << '/' << std::setw(2)
            << tm_time->tm_hour << std::setw(2) << tm_time->tm_min
            << std::setw(2) << tm_time->tm_sec << '.' << std::setw(6)
            << tv.tv_usec << ':';
#endif
  }
  if (g_log_tickcount) {
    // stream_ << TickCount() << ':';
  }
  if (severity_ >= 0) {
    stream_ << GetSeverityName(severity_);
  } else {
    stream_ << "VERBOSE" << -severity_;
  }
  stream_ << ":" << filename << ":" << line << "] ";
  message_start_ = stream_.str().length();
}
void LogMessage::Flush() {
  stream_ << std::endl;
  std::string str_newline(stream_.str());

  if ((GetLogDestination() & LOG_TO_STDERR) != 0) {
#if defined(_WIN32)
    WriteToNativeHandle(GetStdHandle(STD_ERROR_HANDLE), str_newline.data(),
                        str_newline.size());
#else
    WriteToNativeHandle(STDERR_FILENO, str_newline.data(), str_newline.size());
#endif
  }

  if ((GetLogDestination() & LOG_TO_FILE) != 0) {
    if (auto fd = GetLogFileNativeHandle(); fd != INVALID_NATIVE_HANDLE_VALUE) {
#if defined(_WIN32)
      WriteToNativeHandle(fd, str_newline.data(), str_newline.size());
#else
      WriteToNativeHandle(fd, str_newline.data(), str_newline.size());
#endif
    }
  }
}
bool ShouldCreateLogMessage(LogSeverity severity) {
  return severity >= AiArgs::instance().log_level;
}
}  // namespace logging
}  // namespace ai
