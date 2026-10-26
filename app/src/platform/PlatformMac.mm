#include "Platform.h"

#import <AppKit/AppKit.h>

namespace platform {

bool prefersReducedMotion()
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}

} // namespace platform
