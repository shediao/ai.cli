#include "io.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>

#include <limits>
#endif

namespace ai::base {

// return value: -1 on error, >=0 on success
ssize_t write_some(unique_fd const& fd, void const* data, std::size_t size) {
#if defined(_WIN32)
  DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(0x7ffff000u, size));
  DWORD written{0};
  BOOL ok = WriteFile(fd.get(), data, chunk, &written, 0);
  if (!ok) {
    return -1;
  }
  return static_cast<ssize_t>(written);
#else
  ssize_t written = -1;
  const std::size_t chunk =
      std::min<std::size_t>(std::numeric_limits<ssize_t>::max(), size);
  do {
    written = ::write(fd.get(), data, chunk);
  } while (written == -1 && errno == EINTR);
  return written;
#endif
}

bool write_all(unique_fd const& fd, void const* data, std::size_t size) {
  auto* p = static_cast<std::byte const*>(data);
  while (size > 0) {
    const ssize_t written = write_some(fd, p, size);
    if (written <= 0) {
      return false;
    }
    p += written;
    size -= static_cast<std::size_t>(written);
  }
  return true;
}

ssize_t read_some(unique_fd const& fd, void* data, std::size_t size) {
  if (size == 0) {
    return 0;
  }
#if defined(_WIN32)
  DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(0x7ffff000u, size));
  DWORD read{0};
  BOOL ok = ReadFile(fd.get(), data, chunk, &read, 0);
  if (!ok) {
    auto err = GetLastError();
    if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA) {
      return 0;
    }
    return -1;
  }
  return static_cast<ssize_t>(read);
#else
  ssize_t read = -1;
  const std::size_t chunk =
      std::min<std::size_t>(std::numeric_limits<ssize_t>::max(), size);
  do {
    read = ::read(fd.get(), data, chunk);
  } while (read == -1 && errno == EINTR);
  return read;
#endif
}

bool read_exact(unique_fd const& fd, void* data, std::size_t size) {
  auto* p = static_cast<std::byte*>(data);
  while (size > 0) {
    const ssize_t read = read_some(fd, p, size);
    if (read <= 0) {
      return false;
    }
    p += read;
    size -= static_cast<std::size_t>(read);
  }
  return true;
}

static bool is_atty(NativeHandle f) {
#if defined(_WIN32)
  DWORD mode{0};
  return GetConsoleMode(f, &mode);
#else
  return isatty(f);
#endif
}

bool stdin_is_atty() {
#if defined(_WIN32)
  return is_atty(GetStdHandle(STD_INPUT_HANDLE));
#else
  return is_atty(STDIN_FILENO);
#endif
}

bool stdout_is_atty() {
#if defined(_WIN32)
  return is_atty(GetStdHandle(STD_OUTPUT_HANDLE));
#else
  return is_atty(STDOUT_FILENO);
#endif
}

bool stderr_is_atty() {
#if defined(_WIN32)
  return is_atty(GetStdHandle(STD_ERROR_HANDLE));
#else
  return is_atty(STDERR_FILENO);
#endif
}

}  // namespace ai::base
