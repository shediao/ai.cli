#ifndef __AI_CLI_UTILS_H__
#define __AI_CLI_UTILS_H__
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

template <typename...>
using void_t = void;

template <typename T, typename = void_t<>>
struct is_callable : public std::false_type {};

template <typename F>
struct is_callable<F, decltype(std::declval<F>()())> : public std::true_type {};

template <typename T>
constexpr bool is_callable_v = is_callable<T>::value;

template <typename T>
  requires is_callable_v<T>
class AutoRun {
 public:
  AutoRun(T exit_func) : exit_(std::move(exit_func)) {}
  ~AutoRun() { exit_(); }
  AutoRun(AutoRun const &) = delete;
  AutoRun(AutoRun &&) = delete;
  AutoRun &operator=(AutoRun const &) = delete;
  AutoRun &operator=(AutoRun &&) = delete;

 private:
  T exit_{nullptr};
};

class TempFile {
 public:
  TempFile();
  TempFile(std::string const &prefix, std::string const &postfix);
  ~TempFile();
  const std::string &path() const;
  std::optional<std::string> content() const;

 private:
  std::string path_;
};

std::string getTempFilePath(std::string const &prefix,
                            std::string const &postfix);
std::string getUserInputViaEditor();
bool download_image(std::string const &image_url, std::string const &image_path,
                    std::string &memi_type);
std::string getMEMI(std::string const &url);

std::string app_data_dir(const std::string &app,
                         const std::string &author = "");

void write_to_history(nlohmann::json const &chat_history,
                      std::string const &history_file);
std::optional<nlohmann::json> get_last_history(std::string const &history_file);
#endif  // __AI_CLI_UTILS_H__
