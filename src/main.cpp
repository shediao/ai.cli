#include <argparse/argparse.hpp>
#include <cstdlib>

#include "args.h"
#include "chat.h"
#include "curl/curl.h"
#include "models.h"
#include "tools/filesystem.h"

class CurlGlobalInitGuard {
 public:
  CurlGlobalInitGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobalInitGuard() { curl_global_cleanup(); }
};
int main(int argc, char* argv[]) {
#if defined(_WIN32)
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
#endif
  CurlGlobalInitGuard guard;
  auto& args = AiArgs::instance();

  auto& cmd = args.parse(argc, argv);

  if (cmd.command() == "chat") {
    regist_filesystem_tools();
    return chat();
  } else if (cmd.command() == "models") {
    return models();
  } else {
    cmd.print_usage();
  }
  return EXIT_FAILURE;
}
