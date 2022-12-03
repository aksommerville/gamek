#include "linuxglx_internal.h"

#if GAMEK_USE_akx11

/* Window closed.
 */
 
static void _cb_close(void *userdata) {
  linuxglx.terminate++;
}

/* Keyboard events.
 */
 
static int _cb_key(void *userdata,int keycode,int value) {
  if (gamek_inmgr_keyboard_event(linuxglx.inmgr,keycode,value)) return 1;
  return 0;
}

/* Init, akx11.
 */
 
static int linuxglx_video_init_akx11() {
  struct akx11_delegate delegate={
    .close=_cb_close,
    .key=_cb_key,
  };
  
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

#endif

#if GAMEK_USE_akdrm

#include <termios.h>

/* Restore termios if modified (atexit).
 */
 
static struct termios linuxglx_termios_restore={0};
static int linuxglx_termios_modified=0;
 
static void linuxglx_restore_termios() {
  if (linuxglx_termios_modified) {
    tcsetattr(STDIN_FILENO,TCSANOW,&linuxglx_termios_restore);
    linuxglx_termios_modified=0;
  }
}

/* Init, akdrm.
 */
 
static int linuxglx_video_init_akdrm() {
  int reqrate=LINUXGLX_UPDATE_RATE_HZ;
  int reqfbw=gamek_client.fbw;
  int reqfbh=gamek_client.fbh;
  if ((reqfbw<1)||(reqfbh<1)) {
    reqfbw=160;
    reqfbh= 90;
  }
  
  struct akdrm_config config={0};
  if (akdrm_find_device(&config,0,reqrate,1920,1080,reqfbw,reqfbh)<0) {
    fprintf(stderr,"%s: Failed to locate a usable DRM device.\n",linuxglx.exename);
    return -1;
  }
  
  struct akdrm_setup setup={
    .video_mode=AKDRM_VIDEO_MODE_FB_GX,
    #if BYTE_ORDER==BIG_ENDIAN
      .fbfmt=AKDRM_FBFMT_RGBX,
    #else
      .fbfmt=AKDRM_FBFMT_XBGR,
    #endif
    .fbw=reqfbw,
    .fbh=reqfbh,
  };
  
  if (!(linuxglx.akdrm=akdrm_new(&config,&setup))) {
    fprintf(stderr,"%s: Failed to initialize DRM.\n",linuxglx.exename);
    akdrm_config_cleanup(&config);
    return -1;
  }
  akdrm_config_cleanup(&config);
  
  // Validate framebuffer, make sure we tell the truth when exposing it to client.
  int video_mode=akdrm_get_video_mode(linuxglx.akdrm);
  int fbfmt=akdrm_get_fbfmt(linuxglx.akdrm);
  int fbw,fbh;
  akdrm_get_fb_size(&fbw,&fbh,linuxglx.akdrm);
  if (!akdrm_video_mode_is_fb(video_mode)) {
    fprintf(stderr,"%s: Expected a framebuffer video mode from akdrm, got mode %d.\n",linuxglx.exename,video_mode);
    return -1;
  }
  if ((fbw!=reqfbw)||(fbh!=reqfbh)) {
    fprintf(stderr,
      "%s: Requested framebuffer (%d,%d) but got (%d,%d) from akdrm.\n",
      linuxglx.exename,reqfbw,reqfbh,fbw,fbh
    );
    return -1;
  }
  if (fbfmt!=setup.fbfmt) {
    fprintf(stderr,
      "%s: Requested framebuffer format %d but got %d from akdrm.\n",
      linuxglx.exename,setup.fbfmt,fbfmt
    );
    return -1;
  }
  
  linuxglx.fb.w=fbw;
  linuxglx.fb.h=fbh;
  linuxglx.fb.stride=akdrm_fbfmt_measure_stride(fbfmt,fbw);
  linuxglx.fb.fmt=GAMEK_IMGFMT_RGBX;
  linuxglx.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;
  
  /* It's not really a DRM thing per se, but we should assume that we've been launched at the command line,
   * and put that terminal into noncanonical mode in case the user touches her keyboard.
   */
  if (!linuxglx_termios_modified) {
    if (tcgetattr(STDIN_FILENO,&linuxglx_termios_restore)>=0) {
      struct termios termios=linuxglx_termios_restore;
      termios.c_lflag&=~(ICANON|ECHO);
      if (tcsetattr(STDIN_FILENO,TCSANOW,&termios)>=0) {
        linuxglx_termios_modified=1;
        atexit(linuxglx_restore_termios);
        linuxglx.use_stdin=1;
      }
    }
  }
  
  return 0;
}

#endif

/* Init, any driver.
 */
 
int linuxglx_video_init() {
  #if GAMEK_USE_akx11
    if (linuxglx_video_init_akx11()>=0) return 0;
    akx11_del(linuxglx.akx11);
    linuxglx.akx11=0;
  #endif
  #if GAMEK_USE_akdrm
    if (linuxglx_video_init_akdrm()>=0) return 0;
    akdrm_del(linuxglx.akdrm);
    linuxglx.akdrm=0;
  #endif
  fprintf(stderr,"%s: Failed to initialize any video driver.\n",linuxglx.exename);
  return -1;
}

/* Update.
 */
 
int linuxglx_video_update() {
  #if GAMEK_USE_akx11
    if (linuxglx.akx11) {
      return akx11_update(linuxglx.akx11);
    }
  #endif
  return 0;
}

/* Render.
 */
 
void linuxglx_video_render() {
  #if GAMEK_USE_akx11
    if (linuxglx.akx11) {
      if (linuxglx.fb.v=akx11_begin_fb(linuxglx.akx11)) {
        gamek_client.render(&linuxglx.fb);
        akx11_end_fb(linuxglx.akx11,linuxglx.fb.v);
      } else {
        linuxglx.terminate++;
      }
      return;
    }
  #endif
  #if GAMEK_USE_akdrm
    if (linuxglx.akdrm) {
      if (linuxglx.fb.v=akdrm_begin_fb(linuxglx.akdrm)) {
        gamek_client.render(&linuxglx.fb);
        akdrm_end_fb(linuxglx.akdrm,linuxglx.fb.v);
      } else {
        fprintf(stderr,"akdrm_begin_fb failed\n");
        linuxglx.terminate++;
      }
    }
  #endif
}
