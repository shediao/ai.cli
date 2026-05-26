#pragma once
#include <string>
#include <vector>

namespace ai::base {

// Fast file I/O: single-shot binary read/write with preallocated buffer.
#if defined(_WIN32)
std::string acp_to_utf8(std::string_view wstr_view);
std::string utf16_to_utf8(std::wstring_view wstr_view);
std::wstring utf8_to_utf16(std::string_view str_view);
#else
std::string utf16_to_utf8(std::u16string_view u16str_view);
std::u16string utf8_to_utf16(std::string_view str_view);
#endif

std::vector<std::string> split(const std::string& s, char delim);
std::vector<std::wstring> split(const std::wstring& s, wchar_t delim);

std::string utf8_truncate(std::string const& s, size_t max_length);

bool is_utf8(const void* data, size_t len);
bool is_utf16(const void* data, size_t len);

}  // namespace ai::base
