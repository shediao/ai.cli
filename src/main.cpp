#include <argparse.hpp>
#include <cstdlib>

#include "./args.h"
#include "./chat.h"
#include "./models.h"
#include "./tools/filesystem.h"
#include "curl/curl.h"

class CurlGlobalInitGuard {
   public:
    CurlGlobalInitGuard() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInitGuard() { curl_global_cleanup(); }
};
int main(int argc, char* argv[]) {
    // Initialize a guard for CURL global resources, ensuring cleanup on scope exit
    CurlGlobalInitGuard guard;
    // Get the singleton instance of AiArgs for handling command-line arguments
    auto& args = AiArgs::instance();

    // Parse the command-line arguments and get the command object
    auto& cmd = args.parse(argc, argv);
    
    // Check the command type and execute the corresponding functionality
    if (cmd.command() == "chat") {
        // Register filesystem tools before executing the chat functionality
        regist_filesystem_tools();
        return chat(); // Execute the chat command
    } else if (cmd.command() == "models") {
        return models(); // Execute the models command
    } else {
        // Print usage instructions if the command is not recognized
        cmd.print_usage();
    }
    // Return failure if no valid command was executed
    return EXIT_FAILURE;

}
