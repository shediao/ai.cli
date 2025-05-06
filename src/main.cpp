#include <argparse.hpp>

#include "./args.h"
#include "./chat.h"
#include "./models.h"
#include "curl/curl.h"

class CurlGlobalInitGuard {
   public:
    CurlGlobalInitGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInitGuard() { curl_global_cleanup(); }
};

int main(int argc, const char* argv[]) {
    CurlGlobalInitGuard guard;
    auto& args = AiArgs::instance();

    auto& cmd = args.parse(argc, (char**)argv);
    if (cmd.command() == "chat") {
        chat(args);
    } else if (cmd.command() == "models") {
        models(args);
    }
    return EXIT_SUCCESS;
}
