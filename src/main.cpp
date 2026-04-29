#include <argparse/argparse.hpp>
#include <cstdlib>

#include "ai/args.h"
#include "ai/chat.h"
#include "ai/models.h"
#include "ai/tools/bash.h"
#include "ai/tools/default.h"
#include "ai/tools/filesystem.h"
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
  auto& args = AiArgs::instance();

  auto& cmd = args.parse(argc, argv);

  if (cmd.command() == "chat") {
    regist_filesystem_tools();
    regist_bash_tools();
    regist_default_tools();
    return chat();
  } else if (cmd.command() == "models") {
    return models();
  } else {
    cmd.print_usage();
  }
  return EXIT_FAILURE;
}
