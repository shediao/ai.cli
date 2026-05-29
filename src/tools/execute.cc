#include "execute.h"

#include <numeric>
#include <regex>

#include "nlohmann/json.hpp"

namespace ai {

// Apply line-based filters to text. Each filter in the array is applied in
// order. Each filter object must have exactly one of: head, tail, include,
// exclude.
std::string filter_lines(std::string const& text,
                         nlohmann::json const& filters) {
  if (text.empty()) {
    return text;
  }
  if (!filters.is_array()) {
    return text;
  }

  std::vector<std::string_view> lines;
  size_t pos = 0;
  while (pos <= text.size()) {
    auto next = text.find('\n', pos);
    if (next == std::string::npos) {
      // No more newlines. If the text does not end with '\n', the remainder
      // is the final line. Otherwise the trailing empty after '\n' is
      // dropped (matches std::getline semantics).
      if (pos < text.size()) {
        lines.emplace_back(text.data() + pos, text.size() - pos);
      }
      break;
    }
    lines.emplace_back(text.data() + pos, next - pos);
    pos = next + 1;
  }

  // Apply each filter in array order
  for (auto const& filter : filters) {
    if (!filter.is_object()) {
      continue;
    }

    if (filter.contains("exclude") && filter["exclude"].is_string()) {
      std::regex re(filter["exclude"].get<std::string>(),
                    std::regex::ECMAScript);
      std::erase_if(lines, [&re](std::string_view line) {
        return std::regex_search(line.begin(), line.end(), re);
      });
    } else if (filter.contains("include") && filter["include"].is_string()) {
      std::regex re(filter["include"].get<std::string>(),
                    std::regex::ECMAScript);
      std::erase_if(lines, [&re](std::string_view line) {
        return !std::regex_search(line.begin(), line.end(), re);
      });
    } else if (filter.contains("head") && filter["head"].is_number_integer()) {
      auto v = filter["head"].get<int>();
      if (v >= 0 && static_cast<size_t>(v) < lines.size()) {
        lines.resize(static_cast<size_t>(v));
      }
    } else if (filter.contains("tail") && filter["tail"].is_number_integer()) {
      auto v = filter["tail"].get<int>();
      if (v >= 0 && static_cast<size_t>(v) < lines.size()) {
        lines.erase(lines.begin(), lines.end() - static_cast<size_t>(v));
      }
    }
  }

  // Join lines back with newlines
  std::string result;
  result.reserve(std::accumulate(
      lines.begin(), lines.end(), 0,
      [](size_t acc, std::string_view line) { return acc + line.size() + 1; }));
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) {
      result.push_back('\n');
    }
    result.append(lines[i].begin(), lines[i].end());
  }
  return result;
}

void add_filter_parameter(nlohmann::json& schema) {
  auto filter = R"(
{
  "filter": {
    "type": "array",
    "description": "Optional ordered list of filters to apply to command output (both stdout and stderr). Filters are applied in array order. When a command is expected to produce long output (e.g., builds, logs, large directory listings), ALWAYS use filters — especially tail — to limit the output to a manageable size. Avoid returning excessive output that would overflow the context window.",
    "items": {
      "oneOf": [
        {
          "type": "object",
          "properties": {
            "head": {
              "type": "integer",
              "description": "Keep only the first N lines of output."
            }
          },
          "required": ["head"],
          "additionalProperties": false
        },
        {
          "type": "object",
          "properties": {
            "tail": {
              "type": "integer",
              "description": "Keep only the last N lines of output."
            }
          },
          "required": ["tail"],
          "additionalProperties": false
        },
        {
          "type": "object",
          "properties": {
            "include": {
              "type": "string",
              "description": "Only keep lines matching this ECMAScript regex pattern."
            }
          },
          "required": ["include"],
          "additionalProperties": false
        },
        {
          "type": "object",
          "properties": {
            "exclude": {
              "type": "string",
              "description": "Remove lines matching this ECMAScript regex pattern."
            }
          },
          "required": ["exclude"],
          "additionalProperties": false
        }
      ]
    }
  }
}
)"_json;

  schema["parameters"]["properties"].update(filter);
}

}  // namespace ai
