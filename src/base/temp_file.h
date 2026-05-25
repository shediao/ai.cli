#pragma once
#include <optional>
#include <string>

namespace ai::base {
class TempFile {
 public:
  TempFile();
  TempFile(std::string const& prefix, std::string const& postfix);
  ~TempFile();
  const std::string& path() const;
  std::optional<std::string> content() const;

 private:
  std::string path_;
};
std::string getTempFilePath(std::string const& prefix,
                            std::string const& postfix);
}  // namespace ai::base
