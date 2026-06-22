#include <curl/curl.h>

#include <argparse/argparse.hpp>

#include "ai/args.h"
#include "ai/chat.h"
#include "ai/history.h"
#include "ai/models.h"
#include "ai/update.h"
#include "base/scope_exit.h"
#include "base/string.h"

using namespace ai;

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
  auto curl_init_guard =
      ai::base::make_scope_exit([]() { curl_global_cleanup(); });

#if defined(_WIN32)
  auto oldInputCP = GetConsoleCP();
  auto oldOutputCP = GetConsoleOutputCP();

  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);

  auto win_console_cp_guard =
      ai::base::make_scope_exit([oldInputCP, oldOutputCP]() {
        SetConsoleCP(oldInputCP);
        SetConsoleOutputCP(oldOutputCP);
      });
#endif

  auto parser = get_parser();
#if defined(_WIN32)
  std::vector<std::string> utf8_args;
  std::transform(argv, argv + argc, std::back_inserter(utf8_args),
                 [](wchar_t const* w) { return ai::base::utf16_to_utf8(w); });
  std::vector<char const*> utf8_argv;
  std::transform(utf8_args.begin(), utf8_args.end(),
                 std::back_inserter(utf8_argv),
                 [](std::string const& s) { return s.c_str(); });
  utf8_argv.push_back(nullptr);
#endif

  try {
#if defined(_WIN32)
    auto& cmd = parser.parse(argc, utf8_argv.data());
#else
    auto& cmd = parser.parse(argc, argv);
#endif

    if (cmd.command() == "chat") {
      return chat(get_args());
    }
    if (cmd.command() == "models") {
      return models(get_args());
    }
    if (cmd.command() == "history") {
      return history(get_args());
    }
    if (cmd.command() == "update") {
      return update(get_args());
    }
    cmd.print_usage();
  } catch (std::exception const& e) {
    std::cerr << e.what() << "\n";
    exit(EXIT_FAILURE);
  }
  return EXIT_FAILURE;
}
