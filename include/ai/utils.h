#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>  // For HANDLE
#endif

namespace ai::utils {

template <typename...>
using void_t = void;

template <typename T, typename = void_t<>>
struct is_callable : public std::false_type {};

template <typename F>
struct is_callable<F, decltype(std::declval<F>()())> : public std::true_type {};

template <typename T>
constexpr bool is_callable_v = is_callable<T>::value;

std::string app_data_dir(const std::string& app,
                         const std::string& author = "");

std::string format_timestamp(
    std::chrono::time_point<std::chrono::system_clock> =
        std::chrono::system_clock::now(),
    const char* = "%Y/%m/%d %H:%M:%S %z");
std::string format_timenow(const char* = "%Y/%m/%d %H:%M:%S %z");
}  // namespace ai::utils
