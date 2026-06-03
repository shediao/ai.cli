#include <curl/curl.h>
#include <gtest/gtest.h>

#include <iostream>

#include "base/scope_exit.h"

int main(int argc, char** argv) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  auto curl_init_guard =
      ai::base::make_scope_exit([]() { curl_global_cleanup(); });

#if defined(_WIN32)
  auto oldInputCP = GetConsoleCP();
  auto oldOutputCP = GetConsoleOutputCP();

  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);

  auto win_console_cp_guard =
      ai::base::make_scope_exit([oldInputCP, oldOutputCP]() {
        SetConsoleCP(oldInputCP);
        SetConsoleOutputCP(oldOutputCP);
      });
#endif

  std::cout << "Running main() from " << __FILE__ << "\n";
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
