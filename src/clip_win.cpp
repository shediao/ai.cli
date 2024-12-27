
#if defined(_WIN32)
#include <windows.h>

#include <string>

void save_to_clipboard(std::string const& text) {
    OpenClipboard(nullptr);
    EmptyClipboard();
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    memcpy(GlobalLock(hGlobal), text.c_str(), text.size() + 1);
    GlobalUnlock(hGlobal);
    SetClipboardData(CF_TEXT, hGlobal);
    CloseClipboard();
}

std::string load_from_clipboard() {
    OpenClipboard(nullptr);
    HANDLE hData = GetClipboardData(CF_TEXT);
    char* pszText = static_cast<char*>(GlobalLock(hData));
    std::string text(pszText);
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
}
#endif  // _WIN32
