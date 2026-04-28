#include "ai/tool_calls.h"

// ── individual tool (function-level) registry ────────────────────────
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

// ── tool category registry (automated discovery) ─────────────────────
auto& get_tool_category_registry() {
  static std::map<std::string, std::pair<ToolSchemaGetter, ToolRegisterFunc>>
      registry;
  return registry;
}

bool regist_tool_category(std::string const& name,
                          ToolSchemaGetter schema_getter,
                          ToolRegisterFunc register_func) {
  get_tool_category_registry()[name] = {schema_getter, register_func};
  return true;
}

std::set<std::string> get_tool_categories() {
  std::set<std::string> categories;
  for (auto const& [name, _] : get_tool_category_registry()) {
    categories.insert(name);
  }
  return categories;
}

std::string_view get_tool_schema(std::string const& category) {
  auto& registry = get_tool_category_registry();
  auto it = registry.find(category);
  if (it != registry.end()) {
    return it->second.first();
  }
  return {};
}

void register_tool_category_funcs(std::string const& category) {
  auto& registry = get_tool_category_registry();
  auto it = registry.find(category);
  if (it != registry.end()) {
    it->second.second();
  }
}
