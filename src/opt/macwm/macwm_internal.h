#ifndef MACWM_INTERNAL_H
#define MACWM_INTERNAL_H

#include "macwm.h"
#include "AKWindow.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#define MACWM_KEY_LIMIT 16

struct macwm {
  struct macwm_delegate delegate;
  int w,h;
  int fullscreen;
  AKWindow *window;
  int modifiers;
  int keyv[MACWM_KEY_LIMIT];
  int keyc;
  int rendermode;
  int fbw,fbh; // MACWM_RENDERMODE_FRAMEBUFFER only
  int mouse_in_bounds;
};

void macwm_register_key(struct macwm *macwm,int keycode);
void macwm_unregister_key(struct macwm *macwm,int keycode);

@interface AKFramebufferView : NSView {
@public
  const void *fb;
  int read_in_progress;
}
-(id)initWithWidth:(int)width height:(int)height;
@end

@interface AKOpenGLView : NSOpenGLView
-(id)initWithWidth:(int)width height:(int)height;
-(void)beginFrame;
-(void)endFrame;
@end

@interface AKMetalView : MTKView<MTKViewDelegate>
@end

#endif
