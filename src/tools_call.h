#ifndef __AI_CLI_TOOLS_CALL_H__
#define __AI_CLI_TOOLS_CALL_H__
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "nlohmann/json.hpp"

std::optional<std::string> call_tool(std::string const& name,
                                     nlohmann::json const& args);

bool regist_tools_call(
    std::string const& name,
    std::function<std::optional<std::string>(nlohmann::json const& args)>);

#endif  // __AI_CLI_TOOLS_CALL_H__
