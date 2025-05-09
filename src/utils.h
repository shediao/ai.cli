#ifndef __AI_CLI_UTILS_H__
#define __AI_CLI_UTILS_H__
#include <string>

class TempFile {
   public:
    TempFile();
    TempFile(std::string const &prefix, std::string const &postfix);
    ~TempFile();
    const std::string &path() const;
    std::optional<std::string> content() const;

   private:
    std::string path_;
};

std::string getTempFilePath(std::string const &prefix,
                            std::string const &postfix);
std::string getUserInputViaEditor();
bool download_image(std::string const &image_url, std::string const &image_path,
                    std::string &memi_type);

#endif  // __AI_CLI_UTILS_H__
