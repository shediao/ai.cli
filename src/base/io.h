#pragma once
#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "base/scoped_handle.h"

namespace ai::base {
#if defined(_WIN32)
using ssize_t = std::ptrdiff_t;
#endif
ssize_t write_some(scoped_handle const& fd, void const* data, std::size_t size);
bool write_all(scoped_handle const& fd, void const* data, std::size_t size);
ssize_t read_some(scoped_handle const& fd, void* data, std::size_t size);
bool read_exact(scoped_handle const& fd, void* data, std::size_t size);

bool stdin_is_atty();
bool stdout_is_atty();
bool stderr_is_atty();
bool stdin_is_foreground();

}  // namespace ai::base
