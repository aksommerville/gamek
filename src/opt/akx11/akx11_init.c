#include "akx11_internal.h"

/* Initialize display and atoms.
 */
 
static int akx11_init_display(struct akx11 *akx11,const struct akx11_setup *setup) {
  
  if (!(akx11->dpy=XOpenDisplay(setup->display))) return -1;
  akx11->screen=DefaultScreen(akx11->dpy);

  #define GETATOM(tag) akx11->atom_##tag=XInternAtom(akx11->dpy,#tag,0);
  GETATOM(WM_PROTOCOLS)
  GETATOM(WM_DELETE_WINDOW)
  GETATOM(_NET_WM_STATE)
  GETATOM(_NET_WM_STATE_FULLSCREEN)
  GETATOM(_NET_WM_STATE_ADD)
  GETATOM(_NET_WM_STATE_REMOVE)
  GETATOM(_NET_WM_ICON)
  GETATOM(_NET_WM_ICON_NAME)
  GETATOM(_NET_WM_NAME)
  GETATOM(STRING)
  GETATOM(UTF8_STRING)
  GETATOM(WM_CLASS)
  #undef GETATOM
  
  return 0;
}

/* Get the size of the monitor we're going to display on.
 * NOT the size of the logical desktop, if it can be avoided.
 * We don't actually know which monitor will be chosen, and we don't want to force it, so use the smallest.
 */
 
static void akx11_estimate_monitor_size(int *w,int *h,const struct akx11 *akx11) {
  *w=*h=0;
  #if BITS_USE_xinerama
    int infoc=0;
    XineramaScreenInfo *infov=XineramaQueryScreens(akx11->dpy,&infoc);
    if (infov) {
      if (infoc>0) {
        *w=infov[0].width;
        *h=infov[0].height;
        int i=infoc; while (i-->1) {
          if ((infov[i].width<*w)||(infov[i].height<*h)) {
            *w=infov[i].width;
            *h=infov[i].height;
          }
        }
      }
      XFree(infov);
    }
  #endif
  if ((*w<1)||(*h<1)) {
    *w=DisplayWidth(akx11->dpy,0);
    *h=DisplayHeight(akx11->dpy,0);
    if ((*w<1)||(*h<1)) {
      *w=640;
      *h=480;
    }
  }
}

static void akx11_size_window_for_framebuffer(
  struct akx11 *akx11,int fbw,int fbh,int monw,int monh
) {
  akx11->w=(monw*3)>>2;
  akx11->h=(monh*3)>>2;
  int wforh=(fbw*akx11->h)/fbh;
  if (wforh<=akx11->w) {
    akx11->w=wforh;
  } else {
    akx11->h=(fbh*akx11->w)/fbw;
  }
}

static void akx11_size_window_for_monitor(
  struct akx11 *akx11,int monw,int monh
) {
  akx11->w=(monw*3)>>2;
  akx11->h=(monh*3)>>2;
}

/* Init, first stage.
 */
 
int akx11_init_start(struct akx11 *akx11,const struct akx11_setup *setup) {

  if (akx11_init_display(akx11,setup)<0) return -1;
  akx11->dstdirty=1;
  
  // Framebuffer size straight from the caller, and force it positive or zero.
  akx11->fbw=setup->fbw;
  akx11->fbh=setup->fbh;
  if ((akx11->fbw<1)||(akx11->fbh<1)) {
    akx11->fbw=0;
    akx11->fbh=0;
  }
  
  // Caller usually doesn't specify a window size, and we default based on frambuffer and monitor.
  akx11->w=setup->w;
  akx11->h=setup->h;
  if ((akx11->w<1)||(akx11->h<1)) {
    int monw,monh;
    akx11_estimate_monitor_size(&monw,&monh,akx11);
    if (akx11->fbw) akx11_size_window_for_framebuffer(akx11,akx11->fbw,akx11->fbh,monw,monh);
    else akx11_size_window_for_monitor(akx11,monw,monh);
  }
  
  // video_mode defaults based on whether a framebuffer is requested.
  akx11->video_mode=setup->video_mode;
  if (akx11->video_mode==AKX11_VIDEO_MODE_AUTO) {
    // We don't ever recommend FB_PURE; client has to ask for it specifically.
    if (akx11->fbw) {
      akx11->video_mode=AKX11_VIDEO_MODE_FB_GX;
    } else {
      akx11->video_mode=AKX11_VIDEO_MODE_OPENGL;
    }
  }
  
  return 0;
}

/* Initialize window with GX.
 */
 
static GLXFBConfig akx11_get_fbconfig(struct akx11 *akx11) {

  int attrv[]={
    GLX_X_RENDERABLE,1,
    GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,
    GLX_RED_SIZE,8,
    GLX_GREEN_SIZE,8,
    GLX_BLUE_SIZE,8,
    GLX_ALPHA_SIZE,0,
    GLX_DEPTH_SIZE,0,
    GLX_STENCIL_SIZE,0,
    GLX_DOUBLEBUFFER,1,
  0};
  
  if (!glXQueryVersion(akx11->dpy,&akx11->glx_version_major,&akx11->glx_version_minor)) {
    return (GLXFBConfig){0};
  }
  
  int fbc=0;
  GLXFBConfig *configv=glXChooseFBConfig(akx11->dpy,akx11->screen,attrv,&fbc);
  if (!configv||(fbc<1)) return (GLXFBConfig){0};
  GLXFBConfig config=configv[0];
  XFree(configv);
  
  return config;
}
 
static int akx11_init_window_gx_inner(struct akx11 *akx11,const struct akx11_setup *setup,XVisualInfo *vi) {
  
  // Ask for mouse events only if implemented by the delegate. Otherwise they're just noise.
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
    0,
  };
  if (akx11->delegate.mmotion) {
    wattr.event_mask|=PointerMotionMask;
  }
  if (akx11->delegate.mbutton||akx11->delegate.mwheel) {
    wattr.event_mask|=ButtonPressMask|ButtonReleaseMask;
  }
  wattr.colormap=XCreateColormap(akx11->dpy,RootWindow(akx11->dpy,vi->screen),vi->visual,AllocNone);
  
  if (!(akx11->win=XCreateWindow(
    akx11->dpy,RootWindow(akx11->dpy,akx11->screen),
    0,0,akx11->w,akx11->h,0,
    vi->depth,InputOutput,vi->visual,
    CWBorderPixel|CWColormap|CWEventMask,&wattr
  ))) return -1;
  
  return 0;
}
 
static int akx11_init_window_gx(struct akx11 *akx11,const struct akx11_setup *setup) {
  GLXFBConfig fbconfig=akx11_get_fbconfig(akx11);
  XVisualInfo *vi=glXGetVisualFromFBConfig(akx11->dpy,fbconfig);
  if (!vi) return -1;
  int err=akx11_init_window_gx_inner(akx11,setup,vi);
  XFree(vi);
  if (err<0) return -1;
  if (!(akx11->ctx=glXCreateNewContext(akx11->dpy,fbconfig,GLX_RGBA_TYPE,0,1))) return -1;
  glXMakeCurrent(akx11->dpy,akx11->win,akx11->ctx);
  return 0;
}

/* Init public OpenGL context.
 */
 
int akx11_init_opengl(struct akx11 *akx11,const struct akx11_setup *setup) {
  if (akx11_init_window_gx(akx11,setup)<0) return -1;
  return 0;
}

/* Init with OpenGL but a framebuffer to the client.
 */
 
int akx11_init_fb_gx(struct akx11 *akx11,const struct akx11_setup *setup) {
  if (akx11_init_window_gx(akx11,setup)<0) return -1;
  
  // Allocate OpenGL texture.
  glGenTextures(1,&akx11->fbtexid);
  if (!akx11->fbtexid) {
    glGenTextures(1,&akx11->fbtexid);
    if (!akx11->fbtexid) return -1;
  }
  glBindTexture(GL_TEXTURE_2D,akx11->fbtexid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,akx11->fbmagfilter=GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  
  // Allocate framebuffer for exposure to client.
  // We have a strong bias for RGBX, since that is a well-supported OpenGL format.
  akx11->fbfmt=setup->fbfmt;
  if (akx11->fbfmt==AKX11_FBFMT_ANYTHING) {
    #if BYTE_ORDER==LITTLE_ENDIAN
      akx11->fbfmt=AKX11_FBFMT_XBGR;
    #else
      akx11->fbfmt=AKX11_FBFMT_RGBX;
    #endif
  }
  int buffersize=akx11_fbfmt_measure_buffer(akx11->fbfmt,akx11->fbw,akx11->fbh);
  if (buffersize<1) return -1;
  if (!(akx11->fb=calloc(1,buffersize))) return -1;
  
  // If the client framebuffer is not RGBX, allocate a conversion buffer.
  // There are other formats we could upload directly, but I think it won't be very common, and safer to just convert them.
  int isrgbx=0;
  #if BYTE_ORDER==LITTLE_ENDIAN
    if (akx11->fbfmt==AKX11_FBFMT_XBGR) isrgbx=1;
  #else
    if (akx11->fbfmt==AKX11_FBFMT_RGBX) isrgbx=1;
  #endif
  if (!isrgbx) {
    int cvtsize=akx11->fbw*akx11->fbh*4;
    if (!(akx11->fbcvt=malloc(cvtsize))) return -1;
  }
  
  return 0;
}

/* Init public framebuffer with software scaling.
 */
 
static int akx11_init_window_fb_inner(struct akx11 *akx11,const struct akx11_setup *setup) {
  
  // Ask for mouse events only if implemented by the delegate. Otherwise they're just noise.
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
    0,
  };
  if (akx11->delegate.mmotion) {
    wattr.event_mask|=PointerMotionMask;
  }
  if (akx11->delegate.mbutton||akx11->delegate.mwheel) {
    wattr.event_mask|=ButtonPressMask|ButtonReleaseMask;
  }
  
  if (!(akx11->win=XCreateWindow(
    akx11->dpy,RootWindow(akx11->dpy,akx11->screen),
    0,0,akx11->w,akx11->h,0,
    DefaultDepth(akx11->dpy,akx11->screen),InputOutput,CopyFromParent,
    CWBorderPixel|CWEventMask,&wattr
  ))) return -1;
  
  return 0;
}
 
int akx11_init_fb_pure(struct akx11 *akx11,const struct akx11_setup *setup) {

  akx11->scale_limit=setup->scale_limit;
  
  if (akx11_init_window_fb_inner(akx11,setup)<0) return -1;
  
  if (!(akx11->gc=XCreateGC(akx11->dpy,akx11->win,0,0))) return -1;
  
  // Allocate framebuffer for exposure to client.
  // Prefer the X display's format if client didn't request one.
  akx11->fbfmt=setup->fbfmt;
  if (akx11->fbfmt==AKX11_FBFMT_ANYTHING) {
    Visual *visual=DefaultVisual(akx11->dpy,akx11->screen);
         if ((visual->red_mask==0x000000ff)&&(visual->green_mask==0x0000ff00)&&(visual->blue_mask==0x00ff0000)) akx11->fbfmt=AKX11_FBFMT_XBGR;
    else if ((visual->red_mask==0x00ff0000)&&(visual->green_mask==0x0000ff00)&&(visual->blue_mask==0x000000ff)) akx11->fbfmt=AKX11_FBFMT_XRGB;
    else if ((visual->red_mask==0xff000000)&&(visual->green_mask==0x00ff0000)&&(visual->blue_mask==0x0000ff00)) akx11->fbfmt=AKX11_FBFMT_RGBX;
    else if ((visual->red_mask==0x0000ff00)&&(visual->green_mask==0x00ff0000)&&(visual->blue_mask==0xff000000)) akx11->fbfmt=AKX11_FBFMT_BGRX;
    else akx11->fbfmt=AKX11_FBFMT_XRGB;
  }
  int buffersize=akx11_fbfmt_measure_buffer(akx11->fbfmt,akx11->fbw,akx11->fbh);
  if (buffersize<1) return -1;
  if (!(akx11->fb=malloc(buffersize))) return -1;

  return 0;
}

/* Set window title.
 */
 
static void akx11_set_title(struct akx11 *akx11,const char *src) {
  
  // I've seen these properties in GNOME 2, unclear whether they might still be in play:
  XTextProperty prop={.value=(void*)src,.encoding=akx11->atom_STRING,.format=8,.nitems=0};
  while (prop.value[prop.nitems]) prop.nitems++;
  XSetWMName(akx11->dpy,akx11->win,&prop);
  XSetWMIconName(akx11->dpy,akx11->win,&prop);
  XSetTextProperty(akx11->dpy,akx11->win,&prop,akx11->atom__NET_WM_ICON_NAME);
    
  // This one becomes the window title and bottom-bar label, in GNOME 3:
  prop.encoding=akx11->atom_UTF8_STRING;
  XSetTextProperty(akx11->dpy,akx11->win,&prop,akx11->atom__NET_WM_NAME);
    
  // This daffy bullshit becomes the Alt-Tab text in GNOME 3:
  {
    char tmp[256];
    int len=prop.nitems+1+prop.nitems;
    if (len<sizeof(tmp)) {
      memcpy(tmp,prop.value,prop.nitems);
      tmp[prop.nitems]=0;
      memcpy(tmp+prop.nitems+1,prop.value,prop.nitems);
      tmp[prop.nitems+1+prop.nitems]=0;
      prop.value=tmp;
      prop.nitems=prop.nitems+1+prop.nitems;
      prop.encoding=akx11->atom_STRING;
      XSetTextProperty(akx11->dpy,akx11->win,&prop,akx11->atom_WM_CLASS);
    }
  }
}

/* Set window icon.
 */
 
static void akx11_copy_icon_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
  }
}
 
static void akx11_set_icon(struct akx11 *akx11,const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)||(w>AKX11_ICON_SIZE_LIMIT)||(h>AKX11_ICON_SIZE_LIMIT)) return;
  int length=2+w*h;
  long *pixels=malloc(sizeof(long)*length);
  if (!pixels) return;
  pixels[0]=w;
  pixels[1]=h;
  akx11_copy_icon_pixels(pixels+2,rgba,w*h);
  XChangeProperty(akx11->dpy,akx11->win,akx11->atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(unsigned char*)pixels,length);
  free(pixels);
}

/* Init, last stage.
 */

int akx11_init_finish(struct akx11 *akx11,const struct akx11_setup *setup) {

  if (setup->title&&setup->title[0]) {
    akx11_set_title(akx11,setup->title);
  }
  
  if (setup->iconrgba&&(setup->iconw>0)&&(setup->iconh>0)) {
    akx11_set_icon(akx11,setup->iconrgba,setup->iconw,setup->iconh);
  }
  
  if (setup->fullscreen) {
    XChangeProperty(
      akx11->dpy,akx11->win,
      akx11->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&akx11->atom__NET_WM_STATE_FULLSCREEN,1
    );
    akx11->fullscreen=1;
  }
  
  XMapWindow(akx11->dpy,akx11->win);
  XSync(akx11->dpy,0);
  XSetWMProtocols(akx11->dpy,akx11->win,&akx11->atom_WM_DELETE_WINDOW,1);
  
  akx11->cursor_visible=1; // x11's default...
  akx11_show_cursor(akx11,0); // our default

  return 0;
}

/* Reconsider scaling filter (AKX11_VIDEO_MODE_FB_GX only)
 */
 
void akx11_reconsider_scaling_filter(struct akx11 *akx11) {
  if (!akx11->fbtexid) return;
  if ((akx11->fbw<1)||(akx11->fbh<1)) return;
  int xscale=akx11->w/akx11->fbw;
  int yscale=akx11->h/akx11->fbh;
  int scale=(xscale<yscale)?xscale:yscale;
  if (scale<1) return; // Will use MIN filter, which is always LINEAR.
  
  /* An arbitrary choice: How much larger than framebuffer must the window be before we switch to crisp-pixel scaling?
   * With crisp pixels, the user will will see some pixels at (threshold) size, and some at (threshold+1).
   * To my eye, the aliasing in a 1-pixel checkboard is tolerable at 4.
   * 3 is a reasonable choice too, just a little too aliasy in bad cases, in my opinion.
   */
  const int threshold=4;
  
  int filter=(scale<threshold)?GL_LINEAR:GL_NEAREST;
  if (filter==akx11->fbmagfilter) return;
  glBindTexture(GL_TEXTURE_2D,akx11->fbtexid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,akx11->fbmagfilter=filter);
}
