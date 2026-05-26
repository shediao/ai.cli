#pragma once
#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <algorithm>

namespace ai::base {

#if defined(_WIN32)
using NativeHandle = HANDLE;
static inline const NativeHandle INVALID_NATIVE_HANDLE_VALUE =
    INVALID_HANDLE_VALUE;
#else   // _WIN32
using NativeHandle = int;
constexpr NativeHandle INVALID_NATIVE_HANDLE_VALUE = -1;
#endif  // !_WIN32
inline bool invalid_handle(NativeHandle handle) {
  return INVALID_NATIVE_HANDLE_VALUE == handle;
}

inline void close_native_handle(NativeHandle& handle) {
  if (!invalid_handle(handle)) {
#if defined(_WIN32)
    CloseHandle(handle);
#else
    close(handle);
#endif
    handle = INVALID_NATIVE_HANDLE_VALUE;
  }
}

inline NativeHandle dup_native_handle(NativeHandle handle) {
  if (invalid_handle(handle)) {
    return INVALID_NATIVE_HANDLE_VALUE;
  }
#if defined(_WIN32)
  NativeHandle duped = INVALID_NATIVE_HANDLE_VALUE;
  if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duped,
                       0, FALSE, DUPLICATE_SAME_ACCESS)) {
    return INVALID_NATIVE_HANDLE_VALUE;
  }
  return duped;
#else
  return fcntl(handle, F_DUPFD_CLOEXEC, 0);
#endif
}

template <typename T>
struct fd_traits;

template <>
struct fd_traits<NativeHandle> {
  static NativeHandle invalid_value() {
#if defined(_WIN32)
    return INVALID_NATIVE_HANDLE_VALUE;
#else
    return -1;
#endif
  }
};

template <typename T, typename Derived, void (*deleter)(T&) = nullptr,
          typename Trait = fd_traits<T>>
class unique_fd_base {
 public:
  unique_fd_base() : handle_(Trait::invalid_value()) {}
  explicit unique_fd_base(NativeHandle handle) : handle_(handle) {}
  ~unique_fd_base() {
    if (deleter) {
      deleter(handle_);
    }
    handle_ = Trait::invalid_value();
  }
  unique_fd_base(unique_fd_base&& other) noexcept : handle_(other.handle_) {
    other.handle_ = Trait::invalid_value();
  }
  unique_fd_base& operator=(unique_fd_base&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    if (deleter) {
      deleter(handle_);
    }
    handle_ = other.handle_;
    other.handle_ = Trait::invalid_value();
    return *this;
  }
  unique_fd_base(const unique_fd_base&) = delete;
  unique_fd_base& operator=(const unique_fd_base&) = delete;

  NativeHandle get() const { return handle_; }
  explicit operator bool() const { return !invalid_handle(handle_); }
  NativeHandle release() {
    auto handle = handle_;
    handle_ = Trait::invalid_value();
    return handle;
  }
  void reset(NativeHandle handle) {
    if (deleter) {
      deleter(handle_);
    }
    handle_ = handle;
  }

  void swap(Derived& other) noexcept { std::swap(handle_, other.handle_); }

  friend bool operator==(Derived const& lhs, Derived const& rhs) {
    return lhs.handle_ == rhs.handle_;
  }
  friend bool operator!=(Derived const& lhs, Derived const& rhs) {
    return lhs.handle_ != rhs.handle_;
  }
  friend bool operator==(Derived const& lhs, NativeHandle rhs) {
    return lhs.handle_ == rhs;
  }
  friend bool operator!=(Derived const& lhs, NativeHandle rhs) {
    return lhs.handle_ != rhs;
  }
  friend bool operator==(NativeHandle lhs, Derived const& rhs) {
    return lhs == rhs.handle_;
  }
  friend bool operator!=(NativeHandle lhs, Derived const& rhs) {
    return lhs != rhs.handle_;
  }

  friend void swap(Derived& lhs, Derived& rhs) noexcept { lhs.swap(rhs); }

 private:
  NativeHandle handle_;
};

class unique_fd
    : public unique_fd_base<NativeHandle, unique_fd, close_native_handle> {
 public:
  using unique_fd_base::unique_fd_base;

  void close() { reset(fd_traits<NativeHandle>::invalid_value()); }

  unique_fd dup() const {
    auto duped = dup_native_handle(get());
    if (invalid_handle(duped)) {
      return unique_fd{};
    }
    return unique_fd{duped};
  }
};

#if defined(_WIN32)
using ssize_t = std::ptrdiff_t;
#endif
ssize_t write_some(unique_fd const& fd, void const* data, std::size_t size);
bool write_all(unique_fd const& fd, void const* data, std::size_t size);
ssize_t read_some(unique_fd const& fd, void* data, std::size_t size);
bool read_exact(unique_fd const& fd, void* data, std::size_t size);

bool stdin_is_atty();
bool stdout_is_atty();
bool stderr_is_atty();
bool stdin_is_foreground();

}  // namespace ai::base
