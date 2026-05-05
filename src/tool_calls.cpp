#include "ai/tool_calls.h"

#include <iostream>

// ── individual tool (function-level) registry ────────────────────────
static auto& get_all_tools() {
  static std::map<std::string,
                  std::function<std::string(nlohmann::json const& args)>>
      tools;
  return tools;
}
static auto& get_tool_schemas() {
  static std::map<std::string, ToolSchemaGetter> schema;
  return schema;
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

// ── tool category registry (automated discovery) ─────────────────────
std::set<std::string> get_tool_categories() {
  std::set<std::string> categories;

  for (auto const& [category, _] : get_tool_schemas()) {
    categories.insert(category);
  }

  return categories;
}
bool regist_tool_category(std::string const& name,
                          ToolSchemaGetter schema_getter,
                          ToolRegisterFunc register_func) {
  get_tool_schemas()[name] = schema_getter;
  register_func();
  return true;
}

std::string_view get_tool_schema(std::string const& category) {
  if (auto it = get_tool_schemas().find(category);
      it != get_tool_schemas().end()) {
    return it->second();
  }
  return {};
}
