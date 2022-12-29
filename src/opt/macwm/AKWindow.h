#ifndef AKWindow_H
#define AKWindow_H

#import <Cocoa/Cocoa.h>

struct macwm;

@interface AKWindow : NSWindow<NSWindowDelegate> {
}

-(id)initWithOwner:(struct macwm*)macwm title:(const char*)title;

@end

#endif
