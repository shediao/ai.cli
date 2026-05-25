#pragma once

#include <string>

namespace ai::base {

class TempDir {
 public:
  TempDir();
  explicit TempDir(std::string const& prefix);
  ~TempDir();
  const std::string& path() const;

 private:
  std::string path_;
};
std::string getTempDirPath(std::string const& prefix);
}  // namespace ai::base
