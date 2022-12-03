#include "akdrm_internal.h"

/* Cleanup config.
 */
 
void akdrm_config_cleanup(struct akdrm_config *config) {
  if (!config) return;
  if (config->path) free(config->path);
  if (config->mode_name) free(config->mode_name);
}

/* Delete context.
 */
 
static void akdrm_fb_cleanup(struct akdrm *akdrm,struct akdrm_fb *fb) {
  if (fb->fbid) {
    drmModeRmFB(akdrm->fd,fb->fbid);
  }
}
 
void akdrm_del(struct akdrm *akdrm) {
  if (!akdrm) return;

  // If waiting for a page flip, we must let it finish first.
  if ((akdrm->fd>=0)&&akdrm->flip_pending) {
    struct pollfd pollfd={.fd=akdrm->fd,.events=POLLIN|POLLERR|POLLHUP};
    if (poll(&pollfd,1,500)>0) {
      char dummy[64];
      if (read(akdrm->fd,dummy,sizeof(dummy))) ;
    }
  }
  
  if (akdrm->fbtexid) glDeleteTextures(1,&akdrm->fbtexid);
  if (akdrm->eglcontext) eglDestroyContext(akdrm->egldisplay,akdrm->eglcontext);
  if (akdrm->eglsurface) eglDestroySurface(akdrm->egldisplay,akdrm->eglsurface);
  if (akdrm->egldisplay) eglTerminate(akdrm->egldisplay);
  
  akdrm_fb_cleanup(akdrm,akdrm->fbv+0);
  akdrm_fb_cleanup(akdrm,akdrm->fbv+1);

  if (akdrm->crtc_restore) {
    if (akdrm->fd>=0) {
      drmModeCrtcPtr crtc=akdrm->crtc_restore;
      drmModeSetCrtc(
        akdrm->fd,
        crtc->crtc_id,
        crtc->buffer_id,
        crtc->x,
        crtc->y,
        &akdrm->connid,
        1,
        &crtc->mode
      );
    }
    drmModeFreeCrtc(akdrm->crtc_restore);
  }
  
  if (akdrm->fd>=0) close(akdrm->fd);
  
  if (akdrm->clientfb) free(akdrm->clientfb);
  
  free(akdrm);
}

/* New context.
 */

struct akdrm *akdrm_new(
  const struct akdrm_config *config,
  const struct akdrm_setup *setup
) {
  if (!config||!setup) return 0;
  struct akdrm *akdrm=calloc(1,sizeof(struct akdrm));
  if (!akdrm) return 0;
  
  akdrm->fd=-1;
  akdrm->fbw=setup->fbw;
  akdrm->fbh=setup->fbh;
  akdrm->fbfmt=setup->fbfmt;
  akdrm->video_mode=setup->video_mode;
  akdrm->w=config->w;
  akdrm->h=config->h;
  
  if ((akdrm->fbw<1)||(akdrm->fbh<1)) akdrm->fbw=akdrm->fbh=0;
  if (akdrm->video_mode==AKDRM_VIDEO_MODE_AUTO) {
    // We won't give AKDRM_VIDEO_MODE_FB_PURE unless they ask for it.
    if (akdrm->fbw) akdrm->video_mode=AKDRM_VIDEO_MODE_FB_GX;
    else akdrm->video_mode=AKDRM_VIDEO_MODE_OPENGL;
  }
  if (akdrm->fbfmt==AKDRM_FBFMT_ANYTHING) {
    // Serial RGBX if they don't specify (TODO confirm this is agreeable to DRM).
    #if BYTE_ORDER==LITTLE_ENDIAN
      akdrm->fbfmt=AKDRM_FBFMT_XBGR;
    #else
      akdrm->fbfmt=AKDRM_FBFMT_RGBX;
    #endif
  }
  
  // Common initialization. Open device and acquire CRTC.
  if (akdrm_open(akdrm,config)<0) {
    akdrm_del(akdrm);
    return 0;
  }
  
  // Initialize things specific to the render strategy.
  int err=-1;
  switch (akdrm->video_mode) {
    case AKDRM_VIDEO_MODE_OPENGL:
    case AKDRM_VIDEO_MODE_FB_GX: err=akdrm_init_gx(akdrm); break;
    case AKDRM_VIDEO_MODE_FB_PURE: err=akdrm_init_fb(akdrm); break;
  }
  if (err<0) {
    akdrm_del(akdrm);
    return 0;
  }
  
  return akdrm;
}

/* Trivial accessors.
 */

int akdrm_get_video_mode(const struct akdrm *akdrm) {
  return akdrm->video_mode;
}

int akdrm_get_fbfmt(const struct akdrm *akdrm) {
  return akdrm->fbfmt;
}

void akdrm_get_fb_size(int *w,int *h,const struct akdrm *akdrm) {
  *w=akdrm->fbw;
  *h=akdrm->fbh;
}

void akdrm_get_size(int *w,int *h,const struct akdrm *akdrm) {
  *w=akdrm->w;
  *h=akdrm->h;
}

/* Frame control, GX.
 */

int akdrm_begin_gx(struct akdrm *akdrm) {
  if (akdrm->video_mode!=AKDRM_VIDEO_MODE_OPENGL) return -1;
  if (akdrm_gx_begin(akdrm)<0) return -1;
  return 0;
}

void akdrm_end_gx(struct akdrm *akdrm) {
  if (akdrm->video_mode!=AKDRM_VIDEO_MODE_OPENGL) return;
  akdrm_gx_end(akdrm);
}

/* Frame control, FB_GX and FB_PURE.
 */
 
void *akdrm_begin_fb(struct akdrm *akdrm) {
  return akdrm->clientfb;
}

void akdrm_end_fb(struct akdrm *akdrm,void *fb) {
  if (!fb||(fb!=akdrm->clientfb)) return;
  switch (akdrm->video_mode) {
    case AKDRM_VIDEO_MODE_FB_GX: {
        if (akdrm_gx_begin(akdrm)<0) return;
        akdrm_gx_render_fb(akdrm);
        akdrm_gx_end(akdrm);
      } break;
    case AKDRM_VIDEO_MODE_FB_PURE: {
        akdrm_fb_swap(akdrm);
      } break;
  }
}
