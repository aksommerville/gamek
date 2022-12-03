#include "akdrm_internal.h"

/* Extra initialization for FB_GX mode.
 */
 
static int akdrm_init_fb_gx(struct akdrm *akdrm) {

  akdrm->fbdstw=(GLfloat)akdrm->fbw/(GLfloat)akdrm->w;
  akdrm->fbdsth=(GLfloat)akdrm->fbh/(GLfloat)akdrm->h;
  if (akdrm->fbdstw>=akdrm->fbdsth) {//TODO check my algebra
    akdrm->fbdsth/=akdrm->fbdstw;
    akdrm->fbdstw=1.0f;
    fprintf(stderr,"wide case %f,%f\n",akdrm->fbdstw,akdrm->fbdsth);
  } else {
    akdrm->fbdstw/=akdrm->fbdsth;
    akdrm->fbdsth=1.0f;
    fprintf(stderr,"tall case %f,%f\n",akdrm->fbdstw,akdrm->fbdsth);
  }

  int buffersize=akdrm_fbfmt_measure_buffer(akdrm->fbfmt,akdrm->fbw,akdrm->fbh);
  if (buffersize<1) return -1;
  if (!(akdrm->clientfb=malloc(buffersize))) return -1;
  
  eglMakeCurrent(akdrm->egldisplay,akdrm->eglsurface,akdrm->eglsurface,akdrm->eglcontext);
  
  glGenTextures(1,&akdrm->fbtexid);
  if (!akdrm->fbtexid) {
    glGenTextures(1,&akdrm->fbtexid);
    if (!akdrm->fbtexid) return -1;
  }
  glBindTexture(GL_TEXTURE_2D,akdrm->fbtexid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  
  return 0;
}

/* Init GX, main entry point.
 */
 
int akdrm_init_gx(struct akdrm *akdrm) {
  
  const EGLint context_attribs[]={
    EGL_CONTEXT_CLIENT_VERSION,2,
    EGL_NONE,
  };
  
  const EGLint config_attribs[]={
    EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
    EGL_RED_SIZE,8,
    EGL_GREEN_SIZE,8,
    EGL_BLUE_SIZE,8,
    EGL_ALPHA_SIZE,8,
    EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,
    EGL_NONE,
  };
  
  const EGLint surface_attribs[]={
    EGL_WIDTH,akdrm->w,
    EGL_HEIGHT,akdrm->h,
    EGL_NONE,
  };
  
  if (!(akdrm->gbmdevice=gbm_create_device(akdrm->fd))) return -1;
  
  if (!(akdrm->gbmsurface=gbm_surface_create(
    akdrm->gbmdevice,
    akdrm->w,akdrm->h,
    GBM_FORMAT_XRGB8888,
    GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING
  ))) return -1;
  
  PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");
  if (!eglGetPlatformDisplayEXT) return -1;
  
  if (!(akdrm->egldisplay=eglGetPlatformDisplayEXT(
    EGL_PLATFORM_GBM_KHR,akdrm->gbmdevice,0
  ))) return -1;
  
  EGLint major=0,minor=0;
  if (!eglInitialize(akdrm->egldisplay,&major,&minor)) return -1;
  
  if (!eglBindAPI(EGL_OPENGL_API)) return -1;
  //if (!eglBindAPI(EGL_OPENGL_ES_API)) return -1;
  
  EGLConfig eglconfig;
  EGLint n;
  if (!eglChooseConfig(
    akdrm->egldisplay,config_attribs,&eglconfig,1,&n
  )||(n!=1)) return -1;
  
  if (!(akdrm->eglcontext=eglCreateContext(
    akdrm->egldisplay,eglconfig,EGL_NO_CONTEXT,context_attribs
  ))) return -1;
  
  if (!(akdrm->eglsurface=eglCreateWindowSurface(
    akdrm->egldisplay,eglconfig,akdrm->gbmsurface,0
  ))) return -1;
  
  /* In FB_GX mode, as opposed to regular GX, we also create a framebuffer to expose to the client.
   */
  if (akdrm->video_mode==AKDRM_VIDEO_MODE_FB_GX) {
    if (akdrm_init_fb_gx(akdrm)<0) return -1;
  }
  
  return 0;
}

/* Begin frame.
 */
 
int akdrm_gx_begin(struct akdrm *akdrm) {
  eglMakeCurrent(akdrm->egldisplay,akdrm->eglsurface,akdrm->eglsurface,akdrm->eglcontext);
  glViewport(0,0,akdrm->w,akdrm->h);
  return 0;
}

/* Poll file. TODO I'm guessing we need this for FB mode too.
 */
 
static void akdrm_cb_vblank(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {}
 
static void akdrm_cb_page1(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {
  struct akdrm *akdrm=userdata;
  akdrm->flip_pending=0;
}
 
static void akdrm_cb_page2(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,unsigned int ctrcid,void *userdata
) {
  akdrm_cb_page1(fd,seq,times,timeus,userdata);
}
 
static void akdrm_cb_seq(
  int fd,uint64_t seq,uint64_t timeus,uint64_t userdata
) {}
 
static int akdrm_poll_file(struct akdrm *akdrm,int to_ms) {
  struct pollfd pollfd={.fd=akdrm->fd,.events=POLLIN};
  if (poll(&pollfd,1,to_ms)<=0) return 0;
  drmEventContext ctx={
    .version=DRM_EVENT_CONTEXT_VERSION,
    .vblank_handler=akdrm_cb_vblank,
    .page_flip_handler=akdrm_cb_page1,
    .page_flip_handler2=akdrm_cb_page2,
    .sequence_handler=akdrm_cb_seq,
  };
  int err=drmHandleEvent(akdrm->fd,&ctx);
  if (err<0) return -1;
  return 0;
}

/* Swap frames, the EGL part.
 */

static int akdrm_swap_egl(uint32_t *fbid,struct akdrm *akdrm) { 
  eglSwapBuffers(akdrm->egldisplay,akdrm->eglsurface);
  
  struct gbm_bo *bo=gbm_surface_lock_front_buffer(akdrm->gbmsurface);
  if (!bo) return -1;
  
  int handle=gbm_bo_get_handle(bo).u32;
  struct akdrm_fb *fb;
  if (!akdrm->fbv[0].handle) {
    fb=akdrm->fbv;
  } else if (handle==akdrm->fbv[0].handle) {
    fb=akdrm->fbv;
  } else {
    fb=akdrm->fbv+1;
  }
  
  if (!fb->fbid) {
    int width=gbm_bo_get_width(bo);
    int height=gbm_bo_get_height(bo);
    int stride=gbm_bo_get_stride(bo);
    fb->handle=handle;
    if (drmModeAddFB(akdrm->fd,width,height,24,32,stride,fb->handle,&fb->fbid)<0) return -1;
    
    if (akdrm->crtcunset) {
      if (drmModeSetCrtc(
        akdrm->fd,akdrm->crtcid,fb->fbid,0,0,
        &akdrm->connid,1,&akdrm->mode
      )<0) {
        fprintf(stderr,"drmModeSetCrtc: %m\n");
        return -1;
      }
      akdrm->crtcunset=0;
    }
  }
  
  *fbid=fb->fbid;
  if (akdrm->bo_pending) {
    gbm_surface_release_buffer(akdrm->gbmsurface,akdrm->bo_pending);
  }
  akdrm->bo_pending=bo;
  
  return 0;
}

/* End frame.
 */
 
void akdrm_gx_end(struct akdrm *akdrm) {

  // There must be no more than one page flip in flight at a time.
  // If one is pending -- likely -- give it a chance to finish.
  if (akdrm->flip_pending) {
    if (akdrm_poll_file(akdrm,10)<0) return;
    if (akdrm->flip_pending) {
      // Page flip didn't complete after a 10 ms delay? Drop the frame, no worries.
      return;
    }
  }
  akdrm->flip_pending=1;
  
  uint32_t fbid=0;
  if (akdrm_swap_egl(&fbid,akdrm)<0) {
    akdrm->flip_pending=0;
    return;
  }
  
  if (drmModePageFlip(akdrm->fd,akdrm->crtcid,fbid,DRM_MODE_PAGE_FLIP_EVENT,akdrm)<0) {
    fprintf(stderr,"drmModePageFlip: %m\n");
    akdrm->flip_pending=0;
    return;
  }
}

/* Render framebuffer.
 */
 
void akdrm_gx_render_fb(struct akdrm *akdrm) {
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,akdrm->fbtexid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,akdrm->fbw,akdrm->fbh,0,GL_RGBA,GL_UNSIGNED_BYTE,akdrm->clientfb);
  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2i(0,0); glVertex2f(-akdrm->fbdstw, akdrm->fbdsth);
    glTexCoord2i(0,1); glVertex2f(-akdrm->fbdstw,-akdrm->fbdsth);
    glTexCoord2i(1,0); glVertex2f( akdrm->fbdstw, akdrm->fbdsth);
    glTexCoord2i(1,1); glVertex2f( akdrm->fbdstw,-akdrm->fbdsth);
  glEnd();
}
