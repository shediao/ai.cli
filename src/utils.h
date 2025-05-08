#ifndef __AI_CLI_UTILS_H__
#define __AI_CLI_UTILS_H__
#include <string>

std::string getUserInputViaEditor();
bool download_image(std::string const &image_url, std::string const &image_path,
                    std::string &memi_type);

#endif  // __AI_CLI_UTILS_H__
