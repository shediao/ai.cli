#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "ai/function.h"

extern std::string expand_tilde(std::string const& path);

std::string move_file(nlohmann::json const& args) {
  if (!args.is_object()) {
    return "function move_file arguments is invalid: expected a JSON object.";
  }
  if (!args.contains("source")) {
    return "function move_file arguments is invalid: missing required "
           "parameter \"source\".";
  }
  if (!args["source"].is_string()) {
    return "function move_file arguments is invalid: \"source\" must be a "
           "string.";
  }
  if (!args.contains("distination")) {
    return "function move_file arguments is invalid: missing required "
           "parameter \"distination\".";
  }
  if (!args["distination"].is_string()) {
    return "function move_file arguments is invalid: \"distination\" must be "
           "a string.";
  }
  std::string source = args["source"].get<std::string>();
  source = expand_tilde(source);
  std::string distination = args["distination"].get<std::string>();
  distination = expand_tilde(distination);
  print_toolcall_log("move_file",
                     {{"source", source}, {"distination", distination}});
  std::error_code err;
  std::filesystem::rename(source, distination, err);

  if (err) {
    return "Error: " + err.message();
  } else {
    return "Successfully moved " + source + " to " + distination;
  }
}
