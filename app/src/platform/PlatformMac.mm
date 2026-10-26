#include "Platform.h"

#include <QWindow>

#import <AppKit/AppKit.h>

namespace platform {

namespace {

// Mută containerul cu butoanele native ca să ocupe toată înălțimea barei noastre și centrează
// butoanele în el (aceeași tehnică ca `trafficLightPosition` din Electron). AppKit refațe
// așezarea la redimensionare, așa că funcția se reaplică la evenimentele ferestrei.
void layoutTrafficLights(NSWindow* window, CGFloat height)
{
    if ((window.styleMask & NSWindowStyleMaskFullScreen) != 0) {
        return; // în full screen, sistemul gestionează bara
    }
    NSButton* close = [window standardWindowButton:NSWindowCloseButton];
    NSView* container = close.superview.superview;
    if (close == nil || container == nil) {
        return;
    }

    NSRect frame = container.frame;
    frame.size.height = height;
    frame.origin.y = NSHeight(window.frame) - height;
    container.frame = frame;

    for (NSWindowButton type : { NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton }) {
        NSButton* button = [window standardWindowButton:type];
        NSRect b = button.frame;
        b.origin.y = std::round((height - NSHeight(b)) / 2.0);
        button.frame = b;
    }
}

} // namespace

bool prefersReducedMotion()
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}

void integrateTitleBar(QWindow* qwindow, int titleBarHeight)
{
    if (qwindow == nullptr) {
        return;
    }
    NSView* view = reinterpret_cast<NSView*>(qwindow->winId());
    NSWindow* window = view.window;
    if (window == nil) {
        return;
    }

    window.titleVisibility = NSWindowTitleHidden;
    window.titlebarAppearsTransparent = YES;

    const CGFloat height = titleBarHeight;
    layoutTrafficLights(window, height);

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    for (NSNotificationName name : { NSWindowDidResizeNotification, NSWindowDidEndLiveResizeNotification,
             NSWindowDidExitFullScreenNotification, NSWindowDidBecomeKeyNotification,
             NSWindowDidResignKeyNotification }) {
        [center addObserverForName:name
                            object:window
                             queue:[NSOperationQueue mainQueue]
                        usingBlock:^(NSNotification*) {
                          layoutTrafficLights(window, height);
                        }];
    }
    // Prima așezare se face și după ce AppKit termină layout-ul inițial al ferestrei.
    dispatch_async(dispatch_get_main_queue(), ^{
      layoutTrafficLights(window, height);
    });
}

} // namespace platform
