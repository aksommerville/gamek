#include "linux_internal.h"

#if GAMEK_USE_akx11

/* Window closed.
 */
 
static void _cb_close(void *userdata) {
  gamek_linux.terminate++;
}

/* Keyboard events.
 */
 
static int _cb_key(void *userdata,int keycode,int value) {
  if (gamek_inmgr_keyboard_event(gamek_linux.inmgr,keycode,value)) return 1;
  return 0;
}

/* Init, akx11.
 */
 
static int linux_video_init_akx11() {
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
    .fullscreen=gamek_linux.init_fullscreen,
    .video_mode=AKX11_VIDEO_MODE_FB_GX,
    #if BYTE_ORDER==BIG_ENDIAN
      .fbfmt=AKX11_FBFMT_RGBX,
    #else
      .fbfmt=AKX11_FBFMT_XBGR,
    #endif
  };
  if (!(gamek_linux.akx11=akx11_new(&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to initialize GLX.\n",gamek_linux.exename);
    return -1;
  }
  
  // Validate framebuffer, make sure we tell the truth when exposing it to client.
  int video_mode=akx11_get_video_mode(gamek_linux.akx11);
  int fbfmt=akx11_get_fbfmt(gamek_linux.akx11);
  int fbw,fbh;
  akx11_get_fb_size(&fbw,&fbh,gamek_linux.akx11);
  if (!akx11_video_mode_is_fb(video_mode)) {
    fprintf(stderr,"%s: Expected a framebuffer video mode from akx11, got mode %d.\n",gamek_linux.exename,video_mode);
    return -1;
  }
  if ((fbw!=reqfbw)||(fbh!=reqfbh)) {
    fprintf(stderr,
      "%s: Requested framebuffer (%d,%d) but got (%d,%d) from akx11.\n",
      gamek_linux.exename,reqfbw,reqfbh,fbw,fbh
    );
    return -1;
  }
  if (fbfmt!=setup.fbfmt) {
    fprintf(stderr,
      "%s: Requested framebuffer format %d but got %d from akx11.\n",
      gamek_linux.exename,setup.fbfmt,fbfmt
    );
    return -1;
  }
  
  gamek_linux.fb.w=fbw;
  gamek_linux.fb.h=fbh;
  gamek_linux.fb.stride=akx11_fbfmt_measure_stride(fbfmt,fbw);
  gamek_linux.fb.fmt=GAMEK_IMGFMT_RGBX;
  gamek_linux.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;
  
  return 0;
}

#endif

#if GAMEK_USE_akdrm

#include <termios.h>

/* Restore termios if modified (atexit).
 */
 
static struct termios linux_termios_restore={0};
static int linux_termios_modified=0;
 
static void linux_restore_termios() {
  if (linux_termios_modified) {
    tcsetattr(STDIN_FILENO,TCSANOW,&linux_termios_restore);
    linux_termios_modified=0;
  }
}

/* Init, akdrm.
 */
 
static int linux_video_init_akdrm() {
  int reqrate=LINUX_UPDATE_RATE_HZ;
  int reqfbw=gamek_client.fbw;
  int reqfbh=gamek_client.fbh;
  if ((reqfbw<1)||(reqfbh<1)) {
    reqfbw=160;
    reqfbh= 90;
  }
  
  struct akdrm_config config={0};
  if (akdrm_find_device(&config,0,reqrate,1920,1080,reqfbw,reqfbh)<0) {
    fprintf(stderr,"%s: Failed to locate a usable DRM device.\n",gamek_linux.exename);
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
  
  if (!(gamek_linux.akdrm=akdrm_new(&config,&setup))) {
    fprintf(stderr,"%s: Failed to initialize DRM.\n",gamek_linux.exename);
    akdrm_config_cleanup(&config);
    return -1;
  }
  akdrm_config_cleanup(&config);
  
  // Validate framebuffer, make sure we tell the truth when exposing it to client.
  int video_mode=akdrm_get_video_mode(gamek_linux.akdrm);
  int fbfmt=akdrm_get_fbfmt(gamek_linux.akdrm);
  int fbw,fbh;
  akdrm_get_fb_size(&fbw,&fbh,gamek_linux.akdrm);
  if (!akdrm_video_mode_is_fb(video_mode)) {
    fprintf(stderr,"%s: Expected a framebuffer video mode from akdrm, got mode %d.\n",gamek_linux.exename,video_mode);
    return -1;
  }
  if ((fbw!=reqfbw)||(fbh!=reqfbh)) {
    fprintf(stderr,
      "%s: Requested framebuffer (%d,%d) but got (%d,%d) from akdrm.\n",
      gamek_linux.exename,reqfbw,reqfbh,fbw,fbh
    );
    return -1;
  }
  if (fbfmt!=setup.fbfmt) {
    fprintf(stderr,
      "%s: Requested framebuffer format %d but got %d from akdrm.\n",
      gamek_linux.exename,setup.fbfmt,fbfmt
    );
    return -1;
  }
  
  gamek_linux.fb.w=fbw;
  gamek_linux.fb.h=fbh;
  gamek_linux.fb.stride=akdrm_fbfmt_measure_stride(fbfmt,fbw);
  gamek_linux.fb.fmt=GAMEK_IMGFMT_RGBX;
  gamek_linux.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;
  
  /* It's not really a DRM thing per se, but we should assume that we've been launched at the command line,
   * and put that terminal into noncanonical mode in case the user touches her keyboard.
   */
  if (!linux_termios_modified) {
    if (tcgetattr(STDIN_FILENO,&linux_termios_restore)>=0) {
      struct termios termios=linux_termios_restore;
      termios.c_lflag&=~(ICANON|ECHO);
      if (tcsetattr(STDIN_FILENO,TCSANOW,&termios)>=0) {
        linux_termios_modified=1;
        atexit(linux_restore_termios);
        gamek_linux.use_stdin=1;
      }
    }
  }
  
  return 0;
}

#endif

/* Init, any driver.
 */
 
int linux_video_init() {
  #if GAMEK_USE_akx11
    if (linux_video_init_akx11()>=0) return 0;
    akx11_del(gamek_linux.akx11);
    gamek_linux.akx11=0;
  #endif
  #if GAMEK_USE_akdrm
    if (linux_video_init_akdrm()>=0) return 0;
    akdrm_del(gamek_linux.akdrm);
    gamek_linux.akdrm=0;
  #endif
  fprintf(stderr,"%s: Failed to initialize any video driver.\n",gamek_linux.exename);
  return -1;
}

/* Update.
 */
 
int linux_video_update() {
  #if GAMEK_USE_akx11
    if (gamek_linux.akx11) {
      return akx11_update(gamek_linux.akx11);
    }
  #endif
  return 0;
}

/* Render.
 */
 
void linux_video_render() {
  #if GAMEK_USE_akx11
    if (gamek_linux.akx11) {
      if (gamek_linux.fb.v=akx11_begin_fb(gamek_linux.akx11)) {
        gamek_client.render(&gamek_linux.fb);
        akx11_end_fb(gamek_linux.akx11,gamek_linux.fb.v);
      } else {
        gamek_linux.terminate++;
      }
      return;
    }
  #endif
  #if GAMEK_USE_akdrm
    if (gamek_linux.akdrm) {
      if (gamek_linux.fb.v=akdrm_begin_fb(gamek_linux.akdrm)) {
        gamek_client.render(&gamek_linux.fb);
        akdrm_end_fb(gamek_linux.akdrm,gamek_linux.fb.v);
      } else {
        fprintf(stderr,"akdrm_begin_fb failed\n");
        gamek_linux.terminate++;
      }
    }
  #endif
}
