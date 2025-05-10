#ifndef __AI_CLI_CLIPBOARD_H__
#define __AI_CLI_CLIPBOARD_H__

#include <string>

void save_to_clipboard(std::string const& text);
std::string load_from_clipboard();

#endif  // __AI_CLI_CLIPBOARD_H__
