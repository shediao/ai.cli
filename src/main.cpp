#include <argparse/argparse.hpp>
#include <cstdlib>
#include <iterator>
#include <nlohmann/json.hpp>
#include <vector>

#include "ai/args.h"
#include "ai/chat.h"
#include "ai/history.h"
#include "ai/models.h"
#include "ai/update.h"
#include "base/string.h"
#include "curl/curl.h"

using namespace ai;

class CurlGlobalInitGuard {
 public:
  CurlGlobalInitGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobalInitGuard() { curl_global_cleanup(); }
  CurlGlobalInitGuard(CurlGlobalInitGuard const&) = delete;
  CurlGlobalInitGuard& operator=(CurlGlobalInitGuard const&) = delete;
  CurlGlobalInitGuard(CurlGlobalInitGuard&&) = delete;
  CurlGlobalInitGuard& operator=(CurlGlobalInitGuard&&) = delete;
};

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
#if defined(_WIN32)
  SetConsoleOutputCP(CP_UTF8);
#endif
  CurlGlobalInitGuard guard;
  AiArgs args;
  auto parser = get_parser(args);
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
      return chat(args);
    }
    if (cmd.command() == "models") {
      return models(args);
    }
    if (cmd.command() == "history") {
      return history(args);
    }
    if (cmd.command() == "update") {
      return update(args);
    }
    cmd.print_usage();
  } catch (std::exception const& e) {
    std::cerr << e.what() << "\n";
    exit(EXIT_FAILURE);
  }
  return EXIT_FAILURE;
}
