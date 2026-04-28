
#include <string>

// TODO: implement Linux clipboard (requires X11/Wayland integration)
void save_to_clipboard([[maybe_unused]] std::string const& text) {}
std::string load_from_clipboard() { return ""; }
