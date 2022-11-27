#include "linuxglx_internal.h"

/* Window closed.
 */
 
static void _cb_close(void *userdata) {
  linuxglx.terminate++;
}

/* Keyboard events.
 */
 
static int _cb_key(void *userdata,int keycode,int value) {
  if (gamek_inmgr_keyboard_event(linuxglx.inmgr,keycode,value)) return 1;
  if (gamek_client.keyboard_input) {
    if (gamek_client.keyboard_input(keycode,value)) return 1;
  }
  return 0;
}

static void _cb_text(void *userdata,int codepoint) {
  if (!gamek_client.text_input) return;
  gamek_client.text_input(codepoint);
}

/* Mouse events.
 */
 
static void _cb_mmotion(void *userdata,int x,int y) {
  if (!gamek_client.mouse_motion) return;
  akx11_coord_fb_from_win(&x,&y,linuxglx.akx11);
  gamek_client.mouse_motion(x,y);
}

static void _cb_mbutton(void *userdata,int btnid,int value) {
  if (!gamek_client.mouse_button) return;
  gamek_client.mouse_button(btnid,value);
}

static void _cb_mwheel(void *userdata,int dx,int dy) {
  if (!gamek_client.mouse_wheel) return;
  gamek_client.mouse_wheel(dx,dy);
}

/* Init.
 */
 
int linuxglx_video_init() {
  struct akx11_delegate delegate={
    .close=_cb_close,
    .key=_cb_key,
  };
  
  // Mouse and text hooks only get populated if the client app has them.
  // Leaving them unset here may allow X11 to use a tighter event mask.
  if (gamek_client.text_input) delegate.text=_cb_text;
  if (gamek_client.mouse_motion||gamek_client.mouse_button||gamek_client.mouse_wheel) {
    delegate.mmotion=_cb_mmotion;
    delegate.mbutton=_cb_mbutton;
    delegate.mwheel=_cb_mwheel;
  }
  
  int reqfbw=gamek_client.fbw;
  int reqfbh=gamek_client.fbh;
  if ((reqfbw<1)||(reqfbh<1)) {
    reqfbw=160;
    reqfbh= 90;
  }
  
  struct akx11_setup setup={
    .title=gamek_client.title,
    .iconrgba=gamek_client.iconrgba,
    .iconw=gamek_client.iconw,
    .iconh=gamek_client.iconh,
    .fbw=reqfbw,
    .fbh=reqfbh,
    .fullscreen=linuxglx.init_fullscreen,
    .video_mode=AKX11_VIDEO_MODE_FB_GX,
    #if BYTE_ORDER==BIG_ENDIAN
      .fbfmt=AKX11_FBFMT_RGBX,
    #else
      .fbfmt=AKX11_FBFMT_XBGR,
    #endif
  };
  if (!(linuxglx.akx11=akx11_new(&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to initialize GLX.\n",linuxglx.exename);
    return -1;
  }
  
  // Validate framebuffer, make sure we tell the truth when exposing it to client.
  int video_mode=akx11_get_video_mode(linuxglx.akx11);
  int fbfmt=akx11_get_fbfmt(linuxglx.akx11);
  int fbw,fbh;
  akx11_get_fb_size(&fbw,&fbh,linuxglx.akx11);
  if (!akx11_video_mode_is_fb(video_mode)) {
    fprintf(stderr,"%s: Expected a framebuffer video mode from akx11, got mode %d.\n",linuxglx.exename,video_mode);
    return -1;
  }
  if ((fbw!=reqfbw)||(fbh!=reqfbh)) {
    fprintf(stderr,
      "%s: Requested framebuffer (%d,%d) but got (%d,%d) from akx11.\n",
      linuxglx.exename,reqfbw,reqfbh,fbw,fbh
    );
    return -1;
  }
  if (fbfmt!=setup.fbfmt) {
    fprintf(stderr,
      "%s: Requested framebuffer format %d but got %d from akx11.\n",
      linuxglx.exename,setup.fbfmt,fbfmt
    );
    return -1;
  }
  
  linuxglx.fb.w=fbw;
  linuxglx.fb.h=fbh;
  linuxglx.fb.stride=akx11_fbfmt_measure_stride(fbfmt,fbw);
  linuxglx.fb.fmt=GAMEK_IMGFMT_RGBX;
  linuxglx.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;
  
  return 0;
}

/* Update.
 */
 
int linuxglx_video_update() {
  return akx11_update(linuxglx.akx11);
}

/* Cursor.
 */
 
void gamek_platform_show_cursor(uint8_t show) {
  akx11_show_cursor(linuxglx.akx11,show);
}
