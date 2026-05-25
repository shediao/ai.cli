#pragma once
#include <string>
namespace ai::base {
bool download(std::string const& url, std::string const& path,
              std::string& mime_type, std::string const& proxy);

std::string getMIME(std::string const& url, std::string const& proxy);

}  // namespace ai::base
