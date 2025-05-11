#include <argparse.hpp>
#include <cstdlib>

#include "./args.h"
#include "./chat.h"
#include "./models.h"
#include "curl/curl.h"

class CurlGlobalInitGuard {
   public:
    CurlGlobalInitGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInitGuard() { curl_global_cleanup(); }
};

int main(int argc, char* argv[]) {
    CurlGlobalInitGuard guard;
    auto& args = AiArgs::instance();

    auto& cmd = args.parse(argc, argv);
    if (cmd.command() == "chat") {
        return chat(args);
    } else if (cmd.command() == "models") {
        return models(args);
    }
    return EXIT_FAILURE;
}
