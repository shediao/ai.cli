#pragma once
#include <string>
#include <vector>

namespace ai {

namespace base {
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
}  // namespace base

/// @brief ANSI terminal escape code constants for styled console output.
///
/// Usage:
///   std::cout << ai::term::bold << ai::term::green << "OK" << ai::term::reset;
///
/// All codes are constexpr std::string_view values, so they incur no runtime
/// overhead beyond the string literal they represent.

namespace term {

// ============================================================================
// Reset — clears all attributes
// ============================================================================
inline constexpr std::string_view reset = "\033[0m";

// ============================================================================
// Text styles
// ============================================================================
inline constexpr std::string_view bold = "\033[1m";
inline constexpr std::string_view dim = "\033[2m";
inline constexpr std::string_view italic = "\033[3m";
inline constexpr std::string_view underline = "\033[4m";
inline constexpr std::string_view blink = "\033[5m";
inline constexpr std::string_view rapid_blink = "\033[6m";
inline constexpr std::string_view reverse = "\033[7m";
inline constexpr std::string_view hidden = "\033[8m";
inline constexpr std::string_view strikethrough = "\033[9m";

// style-off codes (selective reset)
inline constexpr std::string_view bold_off = "\033[22m";
inline constexpr std::string_view dim_off = "\033[22m";
inline constexpr std::string_view italic_off = "\033[23m";
inline constexpr std::string_view underline_off = "\033[24m";
inline constexpr std::string_view blink_off = "\033[25m";
inline constexpr std::string_view reverse_off = "\033[27m";
inline constexpr std::string_view hidden_off = "\033[28m";
inline constexpr std::string_view strikethrough_off = "\033[29m";

// ============================================================================
// Standard foreground colors (3-bit)
// ============================================================================
inline constexpr std::string_view black = "\033[30m";
inline constexpr std::string_view red = "\033[31m";
inline constexpr std::string_view green = "\033[32m";
inline constexpr std::string_view yellow = "\033[33m";
inline constexpr std::string_view blue = "\033[34m";
inline constexpr std::string_view magenta = "\033[35m";
inline constexpr std::string_view cyan = "\033[36m";
inline constexpr std::string_view white = "\033[37m";

// foreground color-off
inline constexpr std::string_view fg_default = "\033[39m";

// ============================================================================
// Bright / high-intensity foreground colors (3-bit + bold)
// ============================================================================
inline constexpr std::string_view bright_black = "\033[90m";
inline constexpr std::string_view bright_red = "\033[91m";
inline constexpr std::string_view bright_green = "\033[92m";
inline constexpr std::string_view bright_yellow = "\033[93m";
inline constexpr std::string_view bright_blue = "\033[94m";
inline constexpr std::string_view bright_magenta = "\033[95m";
inline constexpr std::string_view bright_cyan = "\033[96m";
inline constexpr std::string_view bright_white = "\033[97m";

// ============================================================================
// Standard background colors (3-bit)
// ============================================================================
inline constexpr std::string_view bg_black = "\033[40m";
inline constexpr std::string_view bg_red = "\033[41m";
inline constexpr std::string_view bg_green = "\033[42m";
inline constexpr std::string_view bg_yellow = "\033[43m";
inline constexpr std::string_view bg_blue = "\033[44m";
inline constexpr std::string_view bg_magenta = "\033[45m";
inline constexpr std::string_view bg_cyan = "\033[46m";
inline constexpr std::string_view bg_white = "\033[47m";

// background color-off
inline constexpr std::string_view bg_default = "\033[49m";

// ============================================================================
// Bright / high-intensity background colors
// ============================================================================
inline constexpr std::string_view bg_bright_black = "\033[100m";
inline constexpr std::string_view bg_bright_red = "\033[101m";
inline constexpr std::string_view bg_bright_green = "\033[102m";
inline constexpr std::string_view bg_bright_yellow = "\033[103m";
inline constexpr std::string_view bg_bright_blue = "\033[104m";
inline constexpr std::string_view bg_bright_magenta = "\033[105m";
inline constexpr std::string_view bg_bright_cyan = "\033[106m";
inline constexpr std::string_view bg_bright_white = "\033[107m";

// ============================================================================
// Convenience combinations of bold + color
// ============================================================================
namespace bold_color {
inline constexpr std::string_view black = "\033[1;30m";
inline constexpr std::string_view red = "\033[1;31m";
inline constexpr std::string_view green = "\033[1;32m";
inline constexpr std::string_view yellow = "\033[1;33m";
inline constexpr std::string_view blue = "\033[1;34m";
inline constexpr std::string_view magenta = "\033[1;35m";
inline constexpr std::string_view cyan = "\033[1;36m";
inline constexpr std::string_view white = "\033[1;37m";
}  // namespace bold_color

// ============================================================================
// Convenience combinations of underline + color
// ============================================================================
namespace underline_color {
inline constexpr std::string_view black = "\033[4;30m";
inline constexpr std::string_view red = "\033[4;31m";
inline constexpr std::string_view green = "\033[4;32m";
inline constexpr std::string_view yellow = "\033[4;33m";
inline constexpr std::string_view blue = "\033[4;34m";
inline constexpr std::string_view magenta = "\033[4;35m";
inline constexpr std::string_view cyan = "\033[4;36m";
inline constexpr std::string_view white = "\033[4;37m";
}  // namespace underline_color

}  // namespace term
}  // namespace ai
