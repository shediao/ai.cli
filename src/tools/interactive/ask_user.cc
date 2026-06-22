#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "ai/function.h"
#include "base/terminal.h"

namespace ai {

namespace detail {
std::string ask_user_impl(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function ask_user arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("question")) {
    return "function ask_user arguments is invalid: missing required "
           "parameter \"question\".";
  }
  if (!args["question"].is_string()) {
    return "function ask_user arguments is invalid: \"question\" must be a "
           "string.";
  }
  if (!args.contains("options")) {
    return "function ask_user arguments is invalid: missing required "
           "parameter \"options\".";
  }
  if (!args["options"].is_array()) {
    return "function ask_user arguments is invalid: \"options\" must be an "
           "array of strings.";
  }

  std::string question = args["question"].get<std::string>();
  std::vector<std::string> options;
  for (auto const& opt : args["options"]) {
    if (!opt.is_string()) {
      return "function ask_user arguments is invalid: all elements in "
             "\"options\" must be strings.";
    }
    options.push_back(opt.get<std::string>());
  }

  if (options.empty()) {
    return "function ask_user arguments is invalid: \"options\" must not be "
           "empty.";
  }

  print_toolcall_log("ask_user", {{"question", question},
                                  {"options", args["options"].dump()}});

  ai::base::Terminal tty;
  if (!tty.available()) {
    return "Error: interactive terminal is not available. Cannot prompt user "
           "for input.";
  }

  std::size_t index = tty.menu(question, options);
  return "User selected: " + options[index];
}

}  // namespace detail

class AskUserFunction : public ai::Function {
 public:
  std::string call(nlohmann::json const& args) override {
    return detail::ask_user_impl(args);
  }
  bool enabled() const override { return ai::base::Terminal().available(); }
  std::string const& category() const override { return category_; }
  nlohmann::json const& schema() const override { return schema_; }
  [[maybe_unused]] static Function* const registered_;

 private:
  std::string category_ = "interactive";
  nlohmann::json schema_ = R"(
{
  "type": "function",
  "name": "ask_user",
  "description": "Ask the user to select from a list of options. Use this when you need user input to decide between multiple choices. The user will be prompted interactively to pick one option. Returns the selected string.",
  "parameters": {
    "type": "object",
    "properties": {
      "question": {
        "type": "string",
        "description": "The question or prompt to display to the user. Describes what the user is choosing and provides context for the decision."
      },
      "options": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "The list of options the user can choose from. The user will be presented with a numbered menu to pick one."
      }
    },
    "required": ["question", "options"]
  }
}
)"_json;
};

AUTO_REGISTER(AskUserFunction);

}  // namespace ai
