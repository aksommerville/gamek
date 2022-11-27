/* akx11.h
 * This API is deliberately arranged to match our hw_video API.
 * Link: -lX11 -lGL -lGLX
 * With BITS_USE_xinerama: -lXinerama
 */
 
#ifndef AKX11_H
#define AKX11_H

struct akx11;

#define AKX11_VIDEO_MODE_AUTO    0
#define AKX11_VIDEO_MODE_OPENGL  1 /* Expose an OpenGL context. */
#define AKX11_VIDEO_MODE_FB_PURE 2 /* Expose a framebuffer and don't touch OpenGL. (scale up in software) */
#define AKX11_VIDEO_MODE_FB_GX   3 /* Expose a framebuffer but use OpenGL to scale up. */

#define AKX11_FBFMT_ANYTHING   0 /* Will normally be some 32-bit RGB variation. */
#define AKX11_FBFMT_XRGB       1 /* The RGB formats are named big-endianly as if you read them as a 32-bit integer. */
#define AKX11_FBFMT_XBGR       2 /* So on a little-endian host, XBGR means R is the first byte serially. */
#define AKX11_FBFMT_RGBX       3
#define AKX11_FBFMT_BGRX       4
#define AKX11_FBFMT_BGR565LE   5
#define AKX11_FBFMT_BGRX4444BE 6
#define AKX11_FBFMT_BGR332     7
#define AKX11_FBFMT_Y1         8
#define AKX11_FBFMT_Y8         9

struct akx11_delegate {
  void *userdata;
  
  void (*close)(void *userdata);
  void (*focus)(void *userdata,int focus);
  void (*resize)(void *userdata,int w,int h);
  
  int (*key)(void *userdata,int keycode,int value);
  void (*text)(void *userdata,int codepoint);
  
  // Leave null if you don't want these, then we don't have to ask X for them:
  void (*mmotion)(void *userdata,int x,int y);
  void (*mbutton)(void *userdata,int btnid,int value);
  void (*mwheel)(void *userdata,int dx,int dy);
};

struct akx11_setup {
  const char *title;
  const void *iconrgba;
  int iconw,iconh;
  int w,h;
  int fbw,fbh;
  int fullscreen;
  int video_mode;
  int fbfmt;
  int scale_limit; // <1=unlimited. Only used by AKX11_VIDEO_MODE_FB_PURE.
  const char *display;
};

void akx11_del(struct akx11 *akx11);

struct akx11 *akx11_new(
  const struct akx11_delegate *delegate,
  const struct akx11_setup *setup
);

void *akx11_get_userdata(const struct akx11 *akx11);
void akx11_get_size(int *w,int *h,const struct akx11 *akx11);
void akx11_get_fb_size(int *w,int *h,const struct akx11 *akx11);
int akx11_get_fullscreen(const struct akx11 *akx11);
int akx11_get_video_mode(const struct akx11 *akx11);
int akx11_get_fbfmt(const struct akx11 *akx11);

int akx11_update(struct akx11 *akx11);

void akx11_set_fullscreen(struct akx11 *akx11,int state);
void akx11_suppress_screensaver(struct akx11 *akx11);

int akx11_begin_gx(struct akx11 *akx11);
void akx11_end_gx(struct akx11 *akx11);
void *akx11_begin_fb(struct akx11 *akx11);
void akx11_end_fb(struct akx11 *akx11,void *fb);

// Cursor is initially hidden.
void akx11_show_cursor(struct akx11 *akx11,int show);

void akx11_coord_fb_from_win(int *x,int *y,const struct akx11 *akx11);
void akx11_coord_win_from_fb(int *x,int *y,const struct akx11 *akx11);

// We take care of this for you, but just in case:
int akx11_codepoint_from_keysym(int keysym);
int akx11_usb_usage_from_keysym(int keysym);

// Currently all supported formats are stored LRTB, so you could get by with just stride.
int akx11_fbfmt_measure_stride(int fbfmt,int w);
int akx11_fbfmt_measure_buffer(int fbfmt,int w,int h);

int akx11_video_mode_is_gx(int video_mode);
int akx11_video_mode_is_fb(int video_mode);

#endif
