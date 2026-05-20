
#include "ai/base64.h"

#include <base64/base64.hpp>

#include "ai/utils.h"

std::string base64_encode(std::string const& input_file) {
  auto content_opt = ai::utils::read_file(input_file);
  if (content_opt.has_value()) {
    auto& content = content_opt.value();
    return base64::encode(std::string_view{content.data(), content.size()});
  }
  return "";
}
