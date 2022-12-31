#include "akx11_internal.h"

/* Delete.
 */

void akx11_del(struct akx11 *akx11) {
  if (!akx11) return;
  
  if (akx11->image) XDestroyImage(akx11->image);
  if (akx11->gc) XFreeGC(akx11->dpy,akx11->gc);
  if (akx11->dpy) XCloseDisplay(akx11->dpy);
  if (akx11->fb) free(akx11->fb);
  if (akx11->fbcvt) free(akx11->fbcvt);
  
  free(akx11);
}

/* New.
 */

struct akx11 *akx11_new(
  const struct akx11_delegate *delegate,
  const struct akx11_setup *setup
) {
  if (!delegate||!setup) return 0;
  struct akx11 *akx11=calloc(1,sizeof(struct akx11));
  if (!akx11) return 0;
  
  akx11->delegate=*delegate;
  
  if (akx11_init_start(akx11,setup)<0) {
    akx11_del(akx11);
    return 0;
  }
  
  int err=-1;
  switch (akx11->video_mode) {
    case AKX11_VIDEO_MODE_OPENGL: err=akx11_init_opengl(akx11,setup); break;
    case AKX11_VIDEO_MODE_FB_PURE: err=akx11_init_fb_pure(akx11,setup); break;
    case AKX11_VIDEO_MODE_FB_GX: err=akx11_init_fb_gx(akx11,setup); break;
  }
  if (err<0) {
    akx11_del(akx11);
    return 0;
  }
  
  if (akx11_init_finish(akx11,setup)<0) {
    akx11_del(akx11);
    return 0;
  }
  
  akx11_reconsider_scaling_filter(akx11);
  
  return akx11;
}

/* Trivial accessors.
 */

void *akx11_get_userdata(const struct akx11 *akx11) {
  if (!akx11) return 0;
  return akx11->delegate.userdata;
}

void akx11_get_size(int *w,int *h,const struct akx11 *akx11) {
  *w=akx11->w;
  *h=akx11->h;
}

void akx11_get_fb_size(int *w,int *h,const struct akx11 *akx11) {
  *w=akx11->fbw;
  *h=akx11->fbh;
}

int akx11_get_fullscreen(const struct akx11 *akx11) {
  return akx11->fullscreen;
}

int akx11_get_video_mode(const struct akx11 *akx11) {
  return akx11->video_mode;
}

int akx11_get_fbfmt(const struct akx11 *akx11) {
  return akx11->fbfmt;
}

/* Fullscreen.
 */

void akx11_set_fullscreen(struct akx11 *akx11,int state) {
  state=state?1:0;
  if (state==akx11->fullscreen) return;
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=akx11->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=akx11->win,
      .data={.l={
        state,
        akx11->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(akx11->dpy,RootWindow(akx11->dpy,akx11->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(akx11->dpy);
  akx11->fullscreen=state;
}

/* Screensaver.
 */
 
void akx11_suppress_screensaver(struct akx11 *akx11) {
  if (akx11->screensaver_suppressed) return;
  XForceScreenSaver(akx11->dpy,ScreenSaverReset);
  akx11->screensaver_suppressed=1;
}

/* Cursor.
 */
 
void akx11_show_cursor(struct akx11 *akx11,int show) {
  if (!akx11) return;
  if (show) {
    if (akx11->cursor_visible) return;
    XDefineCursor(akx11->dpy,akx11->win,0);
    akx11->cursor_visible=1;
  } else {
    if (!akx11->cursor_visible) return;
    XColor color;
    Pixmap pixmap=XCreateBitmapFromData(akx11->dpy,akx11->win,"\0\0\0\0\0\0\0\0",1,1);
    Cursor cursor=XCreatePixmapCursor(akx11->dpy,pixmap,pixmap,&color,&color,0,0);
    XDefineCursor(akx11->dpy,akx11->win,cursor);
    XFreeCursor(akx11->dpy,cursor);
    XFreePixmap(akx11->dpy,pixmap);
    akx11->cursor_visible=0;
  }
}

/* GX frame control.
 */

int akx11_begin_gx(struct akx11 *akx11) {
  if (!akx11->ctx) return -1;
  akx11->screensaver_suppressed=0;
  glXMakeCurrent(akx11->dpy,akx11->win,akx11->ctx);
  glViewport(0,0,akx11->w,akx11->h);
  return 0;
}

void akx11_end_gx(struct akx11 *akx11) {
  if (!akx11->ctx) return;
  glXSwapBuffers(akx11->dpy,akx11->win);
}

/* Recalculate (scale,dstx,dsty,dstw,dsth) if dirty.
 */
 
static void akx11_require_render_bounds_continuous(struct akx11 *akx11) {
  if (!akx11->dstdirty) return;
  akx11->dstdirty=0;
  
  int wforh=(akx11->h*akx11->fbw)/akx11->fbh;
  if (wforh<=akx11->w) {
    akx11->dstw=wforh;
    akx11->dsth=akx11->h;
  } else {
    akx11->dstw=akx11->w;
    akx11->dsth=(akx11->w*akx11->fbh)/akx11->fbw;
  }
  if (akx11->dstw<1) akx11->dstw=1;
  if (akx11->dsth<1) akx11->dsth=1;
  
  // If we are close to the output size but not exact, fudge it a little.
  // It's slightly more efficient to cover the entire client area, lets us avoid a glClear().
  // But unreasonable to expect the user to set it precisely to the pixel.
  // Note that this is in absolute pixels, not a percentage: We're talking about the user's experience of sizing a window.
  // (as opposed to "how much aspect distortion is tolerable?", we prefer to leave that undecided).
  const int snap=10;
  if ((akx11->dstw>=akx11->w-snap)&&(akx11->dsth>=akx11->h-snap)) {
    akx11->dstw=akx11->w;
    akx11->dsth=akx11->h;
  }
  
  akx11->dstx=(akx11->w>>1)-(akx11->dstw>>1);
  akx11->dsty=(akx11->h>>1)-(akx11->dsth>>1);
}
 
static int akx11_require_render_bounds_discrete(struct akx11 *akx11) {
  if (!akx11->dstdirty) return 0;
  akx11->dstdirty=0;
  
  int xscale=akx11->w/akx11->fbw;
  int yscale=akx11->h/akx11->fbh;
  akx11->scale=(xscale<yscale)?xscale:yscale;
  if (akx11->scale<1) akx11->scale=1;
  else if ((akx11->scale_limit>0)&&(akx11->scale>akx11->scale_limit)) akx11->scale=akx11->scale_limit;
  
  akx11->dstw=akx11->fbw*akx11->scale;
  akx11->dsth=akx11->fbh*akx11->scale;
  akx11->dstx=(akx11->w>>1)-(akx11->dstw>>1);
  akx11->dsty=(akx11->h>>1)-(akx11->dsth>>1);
  
  return 1;
}

/* AKX11_VIDEO_MODE_FB_GX: Convert client buffer into conversion buffer if necessary.
 * Returns the final RGBA framebuffer.
 */
 
static void *akx11_prepare_gx_framebuffer(struct akx11 *akx11) {
  if (!akx11->fbcvt) return akx11->fb;
  switch (akx11->fbfmt) {
    #if BYTE_ORDER==LITTLE_ENDIAN
      case AKX11_FBFMT_XRGB: akx11_fbcvt_rgb(akx11->fbcvt,0,8,16,akx11->fb,16,8,0,akx11->fbw,akx11->fbh); break;
      case AKX11_FBFMT_XBGR: akx11_fbcvt_rgb(akx11->fbcvt,0,8,16,akx11->fb,0,8,16,akx11->fbw,akx11->fbh); break;
      case AKX11_FBFMT_RGBX: akx11_fbcvt_rgb(akx11->fbcvt,0,8,16,akx11->fb,24,16,8,akx11->fbw,akx11->fbh); break;
      case AKX11_FBFMT_BGRX: akx11_fbcvt_rgb(akx11->fbcvt,0,8,16,akx11->fb,8,16,24,akx11->fbw,akx11->fbh); break;
    #else
      case AKX11_FBFMT_XRGB: akx11_fbcvt_rgb(akx11->fbcvt,24,16,8,akx11->fb,16,8,0,akx11->fbw,akx11->fbh); break;
      case AKX11_FBFMT_XBGR: akx11_fbcvt_rgb(akx11->fbcvt,24,16,8,akx11->fb,0,8,16,akx11->fbw,akx11->fbh); break;
      case AKX11_FBFMT_RGBX: akx11_fbcvt_rgb(akx11->fbcvt,24,16,8,akx11->fb,24,16,8,akx11->fbw,akx11->fbh); break;
      case AKX11_FBFMT_BGRX: akx11_fbcvt_rgb(akx11->fbcvt,24,16,8,akx11->fb,8,16,24,akx11->fbw,akx11->fbh); break;
    #endif
    case AKX11_FBFMT_BGR565LE: akx11_fbcvt_rgb_bgr565le(akx11->fbcvt,akx11->fb,akx11->fbw,akx11->fbh); break;
    case AKX11_FBFMT_BGRX4444BE: akx11_fbcvt_rgb_bgrx4444be(akx11->fbcvt,akx11->fb,akx11->fbw,akx11->fbh); break;
    case AKX11_FBFMT_BGR332: akx11_fbcvt_rgb_bgr332(akx11->fbcvt,akx11->fb,akx11->fbw,akx11->fbh); break;
    case AKX11_FBFMT_Y1: akx11_fbcvt_rgb_y1(akx11->fbcvt,akx11->fb,akx11->fbw,akx11->fbh); break;
    case AKX11_FBFMT_Y8: akx11_fbcvt_rgb_y8(akx11->fbcvt,akx11->fb,akx11->fbw,akx11->fbh); break;
  }
  return akx11->fbcvt;
}

/* AKX11_VIDEO_MODE_FB_GX: Main rendering.
 * Context must be enabled, and texture uploaded.
 */
 
static void akx11_render_gx_framebuffer(struct akx11 *akx11) {
  if ((akx11->dstw<akx11->w)||(akx11->dsth<akx11->h)) {
    glViewport(0,0,akx11->w,akx11->h);
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glViewport(akx11->dstx,akx11->dsty,akx11->dstw,akx11->dsth);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glEnable(GL_TEXTURE_2D);
  glBegin(GL_TRIANGLE_STRIP);
    glColor4ub(0xff,0xff,0xff,0xff);
    glTexCoord2i(0,0); glVertex2i(-1, 1);
    glTexCoord2i(0,1); glVertex2i(-1,-1);
    glTexCoord2i(1,0); glVertex2i( 1, 1);
    glTexCoord2i(1,1); glVertex2i( 1,-1);
  glEnd();
}

/* AKX11_VIDEO_MODE_FB_PURE: Reallocate scale-up image.
 */
 
static int akx11_require_image(struct akx11 *akx11) {

  // It is very possible for dst bounds to change position but not size.
  // If that was the case, we don't need to do anything here.
  if (akx11->image&&(akx11->image->width==akx11->dstw)&&(akx11->image->height==akx11->dsth)) return 0;
  
  if (akx11->image) {
    XDestroyImage(akx11->image);
    akx11->image=0;
  }
  void *pixels=malloc(akx11->dstw*4*akx11->dsth);
  if (!pixels) return -1;
  if (!(akx11->image=XCreateImage(
    akx11->dpy,DefaultVisual(akx11->dpy,akx11->screen),24,ZPixmap,0,pixels,akx11->dstw,akx11->dsth,32,akx11->dstw*4
  ))) {
    free(pixels);
    return -1;
  }
    
  // And recalculate channel shifts...
  if (!akx11->image->red_mask||!akx11->image->green_mask||!akx11->image->blue_mask) return -1;
  uint32_t m;
  akx11->rshift=0; m=akx11->image->red_mask;   for (;!(m&1);m>>=1,akx11->rshift++) ; if (m!=0xff) return -1;
  akx11->gshift=0; m=akx11->image->green_mask; for (;!(m&1);m>>=1,akx11->gshift++) ; if (m!=0xff) return -1;
  akx11->bshift=0; m=akx11->image->blue_mask;  for (;!(m&1);m>>=1,akx11->bshift++) ; if (m!=0xff) return -1;
  switch (akx11->fbfmt) {
    case AKX11_FBFMT_XRGB: akx11->scale_reformat=(akx11->rshift!=16)||(akx11->gshift!=8)||(akx11->bshift!=0); break;
    case AKX11_FBFMT_XBGR: akx11->scale_reformat=(akx11->rshift!=0)||(akx11->gshift!=8)||(akx11->bshift!=16); break;
    case AKX11_FBFMT_RGBX: akx11->scale_reformat=(akx11->rshift!=24)||(akx11->gshift!=16)||(akx11->bshift!=8); break;
    case AKX11_FBFMT_BGRX: akx11->scale_reformat=(akx11->rshift!=8)||(akx11->gshift!=16)||(akx11->bshift!=24); break;
    default: akx11->scale_reformat=1; break;
  }
  
  return 0;
}

/* AKX11_VIDEO_MODE_FB_PURE: Scale and convert (fb) into (image).
 */
 
static void akx11_fb_scale(struct akx11 *akx11) {

  // These can't be wrong, due to prior assertions.
  // But it would be disastrous if they are, so hey why not be sure.
  if (akx11->image->width!=akx11->fbw*akx11->scale) return;
  if (akx11->image->height!=akx11->fbh*akx11->scale) return;
  
  if (akx11->scale_reformat) switch (akx11->fbfmt) {
    case AKX11_FBFMT_XRGB: {
        akx11_scale_swizzle(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale,
          akx11->rshift,akx11->gshift,akx11->bshift,16,8,0
        );
      } break;
    case AKX11_FBFMT_XBGR: {
        akx11_scale_swizzle(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale,
          akx11->rshift,akx11->gshift,akx11->bshift,0,8,16
        );
      } break;
    case AKX11_FBFMT_RGBX: {
        akx11_scale_swizzle(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale,
          akx11->rshift,akx11->gshift,akx11->bshift,24,16,8
        );
      } break;
    case AKX11_FBFMT_BGRX: {
        akx11_scale_swizzle(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale,
          akx11->rshift,akx11->gshift,akx11->bshift,8,16,24
        );
      } break;
    case AKX11_FBFMT_BGR565LE: {
        akx11_scale_bgr565le(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale,
          akx11->rshift,akx11->gshift,akx11->bshift
        );
      } break;
    case AKX11_FBFMT_BGRX4444BE: {
        akx11_scale_bgrx4444be(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale,
          akx11->rshift,akx11->gshift,akx11->bshift
        );
      } break;
    case AKX11_FBFMT_BGR332: {
        akx11_scale_bgr332(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale,
          akx11->rshift,akx11->gshift,akx11->bshift
        );
      } break;
    case AKX11_FBFMT_Y1: {
        akx11_scale_y1(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale
        );
      } break;
    case AKX11_FBFMT_Y8: {
        akx11_scale_y8(
          akx11->image->data,akx11->fb,
          akx11->fbw,akx11->fbh,akx11->scale
        );
      } break;
  } else akx11_scale_same32(akx11->image->data,akx11->fb,akx11->fbw,akx11->fbh,akx11->scale);
}

/* FB frame control. (both pure and gx-backed)
 */
 
void *akx11_begin_fb(struct akx11 *akx11) {
  akx11->screensaver_suppressed=0;
  return akx11->fb;
}

void akx11_end_fb(struct akx11 *akx11,void *fb) {
  if (fb!=akx11->fb) return;
  if ((akx11->w<1)||(akx11->h<1)) return;
  switch (akx11->video_mode) {
  
    case AKX11_VIDEO_MODE_FB_PURE: {
        if (akx11_require_render_bounds_discrete(akx11)||!akx11->image) {
          if (akx11_require_image(akx11)<0) return;
          XFillRectangle(akx11->dpy,akx11->win,akx11->gc,0,0,akx11->dstx,akx11->h);
          XFillRectangle(akx11->dpy,akx11->win,akx11->gc,0,0,akx11->w,akx11->dsty);
          XFillRectangle(akx11->dpy,akx11->win,akx11->gc,akx11->dstx,akx11->dsty+akx11->dsth,akx11->w,akx11->h);
          XFillRectangle(akx11->dpy,akx11->win,akx11->gc,akx11->dstx+akx11->dstw,akx11->dsty,akx11->w,akx11->h);
        }
        akx11_fb_scale(akx11);
        XPutImage(akx11->dpy,akx11->win,akx11->gc,akx11->image,0,0,akx11->dstx,akx11->dsty,akx11->image->width,akx11->image->height);
      } break;
      
    case AKX11_VIDEO_MODE_FB_GX: {
        akx11_require_render_bounds_continuous(akx11);
        glXMakeCurrent(akx11->dpy,akx11->win,akx11->ctx);
        void *rgba=akx11_prepare_gx_framebuffer(akx11);
        if (rgba) {
          glBindTexture(GL_TEXTURE_2D,akx11->fbtexid);
          glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,akx11->fbw,akx11->fbh,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
          akx11_render_gx_framebuffer(akx11);
          glXSwapBuffers(akx11->dpy,akx11->win);
        }
      } break;
  }
}

/* Convert coordinates.
 */
 
void akx11_coord_fb_from_win(int *x,int *y,const struct akx11 *akx11) {
  if ((akx11->dstw<1)||(akx11->dsth<1)||(akx11->fbw<1)||(akx11->fbh<1)) return;
  (*x)=(((*x)-akx11->dstx)*akx11->fbw)/akx11->dstw;
  (*y)=(((*y)-akx11->dsty)*akx11->fbh)/akx11->dsth;
}

void akx11_coord_win_from_fb(int *x,int *y,const struct akx11 *akx11) {
  if ((akx11->dstw<1)||(akx11->dsth<1)||(akx11->fbw<1)||(akx11->fbh<1)) return;
  *x=((*x)*akx11->dstw)/akx11->fbw+akx11->dstx;
  *y=((*y)*akx11->dsth)/akx11->fbh+akx11->dsty;
}
