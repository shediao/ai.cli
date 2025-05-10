
#include <string>
#if defined(__linux__)
// TODO: impletement
void save_to_clipboard([[maybe_unused]] std::string const& text) {}
std::string load_from_clipboard() { return ""; }
#endif
