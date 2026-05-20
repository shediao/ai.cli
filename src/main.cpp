#include <argparse/argparse.hpp>
#include <cstdlib>
#include <nlohmann/json.hpp>

#include "ai/args.h"
#include "ai/chat.h"
#include "ai/history.h"
#include "ai/models.h"
#include "ai/update.h"
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

  try {
    auto& cmd = parser.parse(argc, argv);

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
