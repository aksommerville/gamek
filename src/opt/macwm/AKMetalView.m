#include "macwm_internal.h"

@implementation AKMetalView

/* in progress
 * https://developer.apple.com/documentation/metal/using_metal_to_draw_a_view_s_contents?language=objc
 * My MacBook Air seems to have just missed the cut-off for Metal.
 */

-(id)init {
  if (!(self=[super init])) return 0;

  self.device=MTLCreateSystemDefaultDevice();
  fprintf(stderr,"device=%p\n",self.device);

  self.clearColor=MTLClearColorMake(0.0,0.5,1.0,1.0);

  self.delegate=self;
  
  return self;
}

-(void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
  //fprintf(stderr,"%s\n",__func__);
}

-(void)drawInMTKView:(MTKView*)view {
  return;
  MTLRenderPassDescriptor *renderPassDescriptor=self.currentRenderPassDescriptor;
  fprintf(stderr,"renderPassDescriptor=%p\n",renderPassDescriptor);
  if (!renderPassDescriptor) return;
}

@end
