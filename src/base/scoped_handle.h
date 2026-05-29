#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <utility>

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
class scoped_handle_base {
 public:
  scoped_handle_base() : handle_(Trait::invalid_value()) {}
  explicit scoped_handle_base(NativeHandle handle) : handle_(handle) {}
  ~scoped_handle_base() {
    if (deleter) {
      deleter(handle_);
    }
    handle_ = Trait::invalid_value();
  }
  scoped_handle_base(scoped_handle_base&& other) noexcept
      : handle_(other.handle_) {
    other.handle_ = Trait::invalid_value();
  }
  scoped_handle_base& operator=(scoped_handle_base&& other) noexcept {
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
  scoped_handle_base(const scoped_handle_base&) = delete;
  scoped_handle_base& operator=(const scoped_handle_base&) = delete;

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

class scoped_handle : public scoped_handle_base<NativeHandle, scoped_handle,
                                                close_native_handle> {
 public:
  using scoped_handle_base::scoped_handle_base;

  void close() { reset(fd_traits<NativeHandle>::invalid_value()); }

  scoped_handle dup() const {
    auto duped = dup_native_handle(get());
    if (invalid_handle(duped)) {
      return scoped_handle{};
    }
    return scoped_handle{duped};
  }
};
}  // namespace ai::base
