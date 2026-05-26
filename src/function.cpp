#include "ai/function.h"

#include <iostream>
#include <memory>
#include <vector>

#include "ai/terminal.h"
#include "ai/utils.h"

void print_toolcall_log(
    std::string_view func_name,
    std::vector<std::pair<std::string, std::string>> const& args) {
  std::cout << ai::term::bold_color::green << "\n● ["
            << ai::utils::format_timestamp() << "]\n";

  std::cout << ai::term::bold_color::green
            << "Function: " << ai::term::bold_color::magenta << func_name
            << "\n";
  for (auto const& [name, value] : args) {
    std::cout << ai::term::bold_color::green << "▶ " << name
              << (value.size() > 128 ? ":\n" : ": ") << ai::term::bright_black
              << value << "\n";
  }
  std::cout << ai::term::reset;
}

namespace ai {

std::string Function::name() const {
  auto const& schema = this->schema();
  if (schema.contains("name")) {
    return schema["name"].get<std::string>();
  }
  return "";
}
std::string Function::description() const {
  auto const& schema = this->schema();
  if (schema.contains("description")) {
    return schema["description"].get<std::string>();
  }
  return "";
}

std::vector<std::unique_ptr<Function>>& functions() {
  static std::vector<std::unique_ptr<Function>> functions;
  return functions;
}
void regist_function(std::unique_ptr<Function> func) {
  if (func && func->enabled()) {
    functions().push_back(std::move(func));
  }
}

std::set<std::string> get_categories() {
  std::set<std::string> result;
  for (auto const& f : functions()) {
    result.insert(f->category());
  }
  return result;
}

nlohmann::json get_tools(std::set<std::string> categories) {
  auto tools = nlohmann::json::array();
  for (auto const& f : functions()) {
    if (categories.find(f->category()) == categories.end()) {
      continue;
    }
    tools.push_back(f->schema());
  }
  return tools;
}

std::string call_tool(std::string const& name, nlohmann::json const& args) {
  for (auto const& f : functions()) {
    if (f->name() == name) {
      return f->call(args);
    }
  }
  return "tool_calls function (" + name + ") not found";
}

}  // namespace ai
