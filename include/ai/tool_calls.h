#pragma once

#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

std::string call_tool(std::string const& name, nlohmann::json const& args);

bool regist_tool_calls(std::string const& name,
                       std::function<std::string(nlohmann::json const& args)>);

// ── Tool category registry (automated tool discovery) ────────────────
using ToolSchemaGetter = std::string_view (*)();
using ToolRegisterFunc = void (*)();

/// Register a tool category (called at static init by each tool module).
bool regist_tool_category(std::string const& name,
                          ToolSchemaGetter schema_getter,
                          ToolRegisterFunc register_func);

/// Return the set of all known tool-category names.
std::set<std::string> get_tool_categories();

/// Return the JSON-schema string for a given tool category.
std::string_view get_tool_schema(std::string const& category);
