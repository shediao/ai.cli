#include "logging.h"

#if defined(__APPLE__)
#include <AppKit/AppKit.h>
#include <string>

static int waitForPasteboardChange(NSPasteboard *pasteboard, NSInteger initialChangeCount) {
  NSDate *timeoutDate = [NSDate dateWithTimeIntervalSinceNow:2.0];  // 2-second timeout
  while ([pasteboard changeCount] == initialChangeCount && [timeoutDate timeIntervalSinceNow] > 0) {
    // Run the loop for a very short interval to allow the main thread to process events
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  }

  // Check if the changeCount was updated
  if ([pasteboard changeCount] == initialChangeCount) {
    return -1;  // Timed out
  }

  return 0;  // Success
}

void save_to_clipboard(std::string const &text) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger changeCount = [pasteboard changeCount];
    NSString *nsString = [NSString stringWithUTF8String:text.c_str()];

    [pasteboard clearContents];
    auto success = [pasteboard setString:nsString forType:NSPasteboardTypeString];
    if (!success) {
      return;
    }
    if (waitForPasteboardChange(pasteboard, changeCount) == -1) {
      LOG(WARNING) << "Timeout waiting for clipboard to update.";
    }
  }
}

std::string load_from_clipboard() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *nsString = [pasteboard stringForType:NSPasteboardTypeString];
    if (nsString == nil) {
      return "";
    }
    return std::string([nsString UTF8String]);
  }
}
#endif  // __APPLE__
