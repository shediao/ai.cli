#include <argparse/argparse.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

#include "ai/args.h"
#include "ai/chat.h"
#include "ai/history.h"
#include "ai/models.h"
#include "ai/tools/bash.h"
#include "ai/tools/default.h"
#include "ai/tools/filesystem.h"
#include "ai/utils.h"
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
  } else if (cmd.command() == "history") {
    auto history_db_path =
        std::filesystem::path(ai::utils::app_data_dir("ai.cli")) /
        "chat_history.db";
    HistoryDB history_db(history_db_path.string());
    int n = args.history_args.n;
    auto sessions = history_db.list_session_infos(n);

    if (sessions.empty()) {
      std::cout << "No chat history found.\n";
      return 0;
    }

    for (auto const& s : sessions) {
      s.print();
    }
    return 0;
  } else {
    cmd.print_usage();
  }
  return EXIT_FAILURE;
}
