#pragma once
#include <optional>
#include <string>
#include <vector>

namespace ai::base {

// Fast file I/O: single-shot binary read/write with preallocated buffer.
#if defined(_WIN32)
std::optional<std::string> toUtf8(const std::string& s);
#endif

std::vector<std::string> split(const std::string& s, char delim);
std::vector<std::wstring> split(const std::wstring& s, wchar_t delim);

std::string utf8_truncate(std::string const& s, size_t max_length);
}  // namespace ai::base
