#include "tool_calls.h"

auto& get_all_tools() {
  static std::map<std::string,
                  std::function<std::string(nlohmann::json const& args)>>
      tools;
  return tools;
}

std::string call_tool(std::string const& name, nlohmann::json const& args) {
  auto it = get_all_tools().find(name);
  if (it != get_all_tools().end()) {
    return it->second(args);
  }
  return "tool_calls function (" + name + ") not found";
}

bool regist_tool_calls(
    std::string const& name,
    std::function<std::string(nlohmann::json const& args)> func) {
  auto ret = get_all_tools().insert_or_assign(name, std::move(func));
  return ret.second;
}
