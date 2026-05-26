#pragma once

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"

#define AUTO_REGISTER(T)                  \
  namespace {                             \
  ai::AutoRegister<T> _auto_register_##T; \
  }

namespace ai {
class Function {
 public:
  virtual ~Function() {}
  virtual std::string call(nlohmann::json const& args) = 0;

  virtual bool enabled() const { return true; }
  virtual std::string const& category() const = 0;
  std::string name() const;
  std::string description() const;
  virtual nlohmann::json const& schema() const = 0;
};

template <typename T>
class AutoRegister {
 public:
  AutoRegister() { regist_function(std::make_unique<T>()); }
};

void regist_function(std::unique_ptr<Function> func);
std::set<std::string> get_categories();
nlohmann::json get_tools(std::set<std::string> categories);
std::string call_tool(std::string const& name, nlohmann::json const& args);

}  // namespace ai
void print_toolcall_log(
    std::string_view func_name,
    std::vector<std::pair<std::string, std::string>> const& args);
