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
  set_ai_args(args);

  auto& cmd = args.parse(argc, argv);

  if (cmd.command() == "chat") {
    return chat(args);
  } else if (cmd.command() == "models") {
    return models(args);
  } else if (cmd.command() == "history") {
    return history(args);
  } else if (cmd.command() == "update") {
    return update(args);
  } else {
    cmd.print_usage();
  }
  return EXIT_FAILURE;
}
