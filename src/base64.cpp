
#include "base64.h"

#include <base64/base64.hpp>
#include <fstream>

std::string base64_encode(std::string const& input_file) {
  std::ifstream input{input_file};
  if (input.is_open()) {
    std::vector<char> content{std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>()};

    return base64::encode(std::string_view{content.data(), content.size()});
  } else {
    return "";
  }
}
