#ifndef AKX11_INTERNAL_H
#define AKX11_INTERNAL_H

#include "akx11.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <limits.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <GL/gl.h>

// Required only for making intelligent initial-size decisions in a multi-monitor setting.
// apt install libxinerama-dev
#ifndef BITS_USE_xinerama
  #define BITS_USE_xinerama 1
#endif
#if BITS_USE_xinerama
  #include <X11/extensions/Xinerama.h>
#endif

#define KeyRepeat (LASTEvent+2)
#define AKX11_KEY_REPEAT_INTERVAL 10
#define AKX11_ICON_SIZE_LIMIT 64

struct akx11 {
  struct akx11_delegate delegate;
  
  Display *dpy;
  int screen;
  Window win;
  GC gc;
  int w,h;
  int fullscreen;
  int video_mode;
  int fbfmt;

  GLXContext ctx;
  int glx_version_minor,glx_version_major;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  Atom atom__NET_WM_ICON;
  Atom atom__NET_WM_ICON_NAME;
  Atom atom__NET_WM_NAME;
  Atom atom_WM_CLASS;
  Atom atom_STRING;
  Atom atom_UTF8_STRING;
  
  int screensaver_suppressed;
  int focus;
  int cursor_visible;
  
  // Used by typical GX and FB modes.
  GLuint fbtexid;
  uint8_t *fb;
  int fbw,fbh;
  int dstdirty; // Nonzero to recalculate bounds next render.
  int dstx,dsty,dstw,dsth;
  
  // Conversion buffer for AKX11_VIDEO_MODE_FB_GX. Always serial RGBX, same dimensions as (fb).
  void *fbcvt;
  
  // Scale-up image, used by AKX11_VIDEO_MODE_FB_PURE.
  XImage *image;
  int rshift,gshift,bshift;
  int scale_reformat; // if zero, (image) happens to be the public framebuffer format.
  int scale;
  int scale_limit;
};

/* Start establishes (video_mode,dpy,screen,atom_*).
 * The mode-specific initializers, call just one, make (win,fb,image), whatever it needs.
 * Finish sets window properties. Title, cursor, etc.
 */
int akx11_init_start(struct akx11 *akx11,const struct akx11_setup *setup);
int akx11_init_opengl(struct akx11 *akx11,const struct akx11_setup *setup);
int akx11_init_fb_pure(struct akx11 *akx11,const struct akx11_setup *setup);
int akx11_init_fb_gx(struct akx11 *akx11,const struct akx11_setup *setup);
int akx11_init_finish(struct akx11 *akx11,const struct akx11_setup *setup);

// To serial RGBX for OpenGL.
void akx11_fbcvt_rgb(void *dst,uint8_t drs,uint8_t dgs,uint8_t dbs,const void *src,uint8_t srs,uint8_t sgs,uint8_t sbs,int w,int h);
void akx11_fbcvt_rgb_bgr565le(void *dst,const void *src,int w,int h);
void akx11_fbcvt_rgb_bgrx4444be(void *dst,const void *src,int w,int h);
void akx11_fbcvt_rgb_bgr332(void *dst,const void *src,int w,int h);
void akx11_fbcvt_rgb_y1(void *dst,const void *src,int w,int h);
void akx11_fbcvt_rgb_y8(void *dst,const void *src,int w,int h);

// To native RGBX for X11.
void akx11_scale_swizzle(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs,uint8_t srs,uint8_t sgs,uint8_t sbs);
void akx11_scale_bgr565le(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs);
void akx11_scale_bgrx4444be(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs);
void akx11_scale_bgr332(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs);
void akx11_scale_y1(void *dst,const void *src,int w,int h,int scale);
void akx11_scale_y8(void *dst,const void *src,int w,int h,int scale);
void akx11_scale_same32(void *dst,const void *src,int w,int h,int scale);

#endif
