
#include <argparse.hpp>

#include "args.h"
#include "chat.h"

int main(int argc, const char* argv[]) {
    auto& args = AiArgs::instance();

    auto& cmd = args.parse(argc, (char**)argv);
    if (cmd.command() == "chat") {
        chat(args);
    }
    return EXIT_SUCCESS;
}
