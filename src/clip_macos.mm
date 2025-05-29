#if defined(__APPLE__)
#include <AppKit/AppKit.h>
#include <string>

void save_to_clipboard(std::string const &text) {
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  NSString *nsString = [NSString stringWithUTF8String:text.c_str()];
  [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
  [pasteboard setString:nsString forType:NSPasteboardTypeString];
}

std::string load_from_clipboard() {
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  NSString *nsString = [pasteboard stringForType:NSPasteboardTypeString];
  return std::string([nsString UTF8String]);
}
#endif  // __APPLE__
