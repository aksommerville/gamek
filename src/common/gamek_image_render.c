#include "gamek_image.h"
#include <string.h>
#include <stdio.h>

/* Clear.
 */
 
void gamek_image_clear(struct gamek_image *image) {
  if (!(image->flags&GAMEK_IMAGE_FLAG_WRITEABLE)) return;
  int16_t c=image->w;
  switch (image->fmt) {
    case GAMEK_IMGFMT_RGBX: c<<=2; break;
    default: return;
  }
  if (c==image->stride) {
    memset(image->v,0,c*image->h);
  } else {
    char *row=image->v;
    int16_t i=image->h;
    for (;i-->0;row+=image->stride) {
      memset(row,0,c);
    }
  }
}

/* Fill rect.
 */

void gamek_image_fill_rect(
  struct gamek_image *image,
  int16_t x,int16_t y,int16_t w,int16_t h,
  uint32_t pixel
) {
  if (!(image->flags&GAMEK_IMAGE_FLAG_WRITEABLE)) return;
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>image->w-w) w=image->w-x;
  if (y>image->h-h) h=image->h-y;
  if ((w<1)||(h<1)) return;
  switch (image->fmt) {
    
    case GAMEK_IMGFMT_RGBX: {
        uint32_t *row=(uint32_t*)((char*)image->v+y*image->stride)+x;
        int16_t wstride=image->stride>>2;
        for (;h-->0;row+=wstride) {
          uint32_t *p=row;
          int16_t xi=w;
          for (;xi-->0;p++) *p=pixel;
        }
      } break;
      
  }
}

/* Clip one rectangle against another.
 * Where (a) exceeds (b), write the amount of excess into (dst): [left,right,top,bottom]
 */
 
static void gamek_image_clip(
  int16_t *dst,
  int16_t ax,int16_t ay,int16_t aw,int16_t ah,
  int16_t bw,int16_t bh
) {
  if (ax<0) dst[0]=-ax; else dst[0]=0;
  if (ay<0) dst[2]=-ay; else dst[2]=0;
  if (ax>bw-aw) dst[1]=ax+aw-bw; else dst[1]=0;
  if (ay>bh-ah) dst[3]=ay+ah-bh; else dst[3]=0;
}

/* Inner blit.
 */
 
static void gamek_image_blit_inner_unchecked(
  struct gamek_image *dst,int16_t dstx,int16_t dsty,
  const struct gamek_image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,uint8_t xform
) {
  struct gamek_image_iterator dstiter,srciter;
  if (xform&GAMEK_XFORM_SWAP) {
    if (!gamek_image_iterator_init(&dstiter,dst,dstx,dsty,h,w,xform)) return;
  } else {
    if (!gamek_image_iterator_init(&dstiter,dst,dstx,dsty,w,h,xform)) return;
  }
  if (!gamek_image_iterator_init(&srciter,(struct gamek_image*)src,srcx,srcy,w,h,0)) return;
  
  if (src->flags&GAMEK_IMAGE_FLAG_TRANSPARENT) {
    do {
      uint32_t pixel=gamek_image_iterator_read(&srciter);
      if (pixel) {
        gamek_image_iterator_write(&dstiter,pixel);
      }
    } while (gamek_image_iterator_next(&dstiter)&&gamek_image_iterator_next(&srciter));
  } else {
    do {
      uint32_t pixel=gamek_image_iterator_read(&srciter);
      gamek_image_iterator_write(&dstiter,pixel);
    } while (gamek_image_iterator_next(&dstiter)&&gamek_image_iterator_next(&srciter));
  }
}
 
static void gamek_image_blit_inner_checked(
  struct gamek_image *dst,int16_t dstx,int16_t dsty,
  const struct gamek_image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,uint8_t xform
) {
  struct gamek_image_iterator dstiter,srciter;
  if (xform&GAMEK_XFORM_SWAP) {
    if (!gamek_image_iterator_init(&dstiter,dst,dstx,dsty,h,w,xform)) return;
  } else {
    if (!gamek_image_iterator_init(&dstiter,dst,dstx,dsty,w,h,xform)) return;
  }
  if (!gamek_image_iterator_init(&srciter,(struct gamek_image*)src,srcx,srcy,w,h,0)) return;
  
  if (src->flags&GAMEK_IMAGE_FLAG_TRANSPARENT) {
    do {
      if (
        (srciter.x>=0)&&(srciter.y>=0)&&(srciter.x<src->w)&&(srciter.y<src->h)&&
        (dstiter.x>=0)&&(dstiter.y>=0)&&(dstiter.x<dst->w)&&(dstiter.y<dst->h)
      ) {
        uint32_t pixel=gamek_image_iterator_read(&srciter);
        if (pixel) {
          gamek_image_iterator_write(&dstiter,pixel);
        }
      }
    } while (gamek_image_iterator_next(&dstiter)&&gamek_image_iterator_next(&srciter));
  } else {
    do {
      if (
        (srciter.x>=0)&&(srciter.y>=0)&&(srciter.x<src->w)&&(srciter.y<src->h)&&
        (dstiter.x>=0)&&(dstiter.y>=0)&&(dstiter.x<dst->w)&&(dstiter.y<dst->h)
      ) {
        uint32_t pixel=gamek_image_iterator_read(&srciter);
        gamek_image_iterator_write(&dstiter,pixel);
      }
    } while (gamek_image_iterator_next(&dstiter)&&gamek_image_iterator_next(&srciter));
  }
}

/* Blit.
 */

void gamek_image_blit(
  struct gamek_image *dst,int16_t dstx,int16_t dsty,
  const struct gamek_image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,uint8_t xform
) {
  if (!dst||!(dst->flags&GAMEK_IMAGE_FLAG_WRITEABLE)) return;
  if (!src) return;
  
  /* Clipping when xforms in play is a real pain.
   * We'll assume that most blits are fully in bounds and try to optimize for that.
   * If they're OOB by even one pixel, we'll do an expensive and redundant per-pixel bounds check.
   * My assumption: OOB blits are uncommon enough that it's not worth the crazy amount of logic we'd need to clip correctly.
   * If there's no xform, clipping is easy so we do it. No xform is by far the likeliest case, too.
   */
  if (!xform) {
    if (dstx<0) { srcx-=dstx; w+=dstx; dstx=0; }
    if (dsty<0) { srcy-=dsty; h+=dsty; dsty=0; }
    if (srcx<0) { dstx-=srcx; w+=srcx; srcx=0; }
    if (srcy<0) { dsty-=srcy; h+=srcy; srcy=0; }
    if (dstx>dst->w-w) w=dst->w-dstx;
    if (dsty>dst->h-h) h=dst->h-dsty;
    if (srcx>src->w-w) w=src->w-srcx;
    if (srcy>src->h-h) h=src->h-srcy;
    if ((w<1)||(h<1)) return;
    // ok we are clipped and nonzero
  } else {
    if ((w<1)||(h<1)) return;
    uint8_t inbounds=1;
    if (dstx<0) inbounds=0;
    else if (dsty<0) inbounds=0;
    else if (srcx<0) inbounds=0;
    else if (srcy<0) inbounds=0;
    else {
      // (src) uses (w,h) regardless of SWAP.
      if ((srcx>src->w-w)||(srcy>src->h-h)) inbounds=0;
      // (dst) swaps the sizes according to xform.
      else if (xform&GAMEK_XFORM_SWAP) {
        if ((dstx>dst->w-h)||(dsty>dst->h-w)) inbounds=0;
      } else {
        if ((dstx>dst->w-w)||(dsty>dst->h-h)) inbounds=0;
      }
    }
    if (!inbounds) {
      gamek_image_blit_inner_checked(dst,dstx,dsty,src,srcx,srcy,w,h,xform);
      return;
    }
    // ok we are fully in bounds
  }
  gamek_image_blit_inner_unchecked(dst,dstx,dsty,src,srcx,srcy,w,h,xform);
}

/* Render a glyph.
 */

void gamek_image_render_glyph(
  struct gamek_image *image,int16_t dstx,int16_t dsty,
  uint32_t src,uint8_t w,uint8_t h,
  uint32_t pixel
) {
  if (!image||!(image->flags&GAMEK_IMAGE_FLAG_WRITEABLE)) return;
  if (!w||!h||!src) return;
  uint8_t srcstride=w;
  
  if ((dstx<0)||(dsty<0)||(dstx>image->w-w)||(dsty>image->h-h)) {
    if (dstx<0) { 
      if (dstx<=-w) return;
      w+=dstx;
      dstx=0;
    }
    if (dsty<0) {
      if (dsty<=-h) return;
      h+=dsty;
      dsty=0;
    }
    if (dstx>=image->w) return;
    if (dsty>=image->h) return;
    if (dstx>image->w-w) w=image->w-dstx;
    if (dsty>image->h-h) h=image->h-dsty;
  }
  
  uint8_t bytesperpixel;
  switch (image->fmt) {
    case GAMEK_IMGFMT_RGBX: bytesperpixel=4; break;
    default: return;
  }
  uint8_t *dstrow=(uint8_t*)image->v+image->stride*dsty+dstx*bytesperpixel;
  for (;h-->0;dstrow+=image->stride,src<<=srcstride) {
    uint32_t mask=0x80000000;
    uint8_t xi=w;
    switch (bytesperpixel) {
      case 1: break;//TODO
      case 2: break;//TODO
      case 4: {
          uint32_t *dstp=(uint32_t*)dstrow;
          for (;xi-->0;dstp++,mask>>=1) if (src&mask) *dstp=pixel;
        } break;
    }
  }
}

/* Initialize iterator.
 */

uint8_t gamek_image_iterator_init(
  struct gamek_image_iterator *iter,
  struct gamek_image *image,
  int16_t x,int16_t y,int16_t w,int16_t h,
  uint8_t xform
) {
  if ((w<1)||(h<1)) return 0;
  
  int16_t bytesperpixel;
  switch (image->fmt) {
    case GAMEK_IMGFMT_RGBX: bytesperpixel=4; break;
    default: return 0;
  }
  
  switch (xform) {
    #define X GAMEK_XFORM_XREV|
    #define Y GAMEK_XFORM_YREV|
    #define S GAMEK_XFORM_SWAP|
    #define D(minx,miny,majx,majy) iter->minor.dx=minx; iter->minor.dy=miny; iter->major.dx=majx; iter->major.dy=majy;
    case       0: D( 1, 0, 0, 1)                 break;
    case X     0: D(-1, 0, 0, 1) x+=w-1;         break;
    case   Y   0: D( 1, 0, 0,-1)         y+=h-1; break;
    case X Y   0: D(-1, 0, 0,-1) x+=w-1; y+=h-1; break;
    case     S 0: D( 0, 1, 1, 0)                 break;
    case X   S 0: D( 0,-1, 1, 0)         y+=h-1; break;
    case   Y S 0: D( 0, 1,-1, 0) x+=w-1;         break;
    case X Y S 0: D( 0,-1,-1, 0) x+=w-1; y+=h-1; break;
    #undef X
    #undef Y
    #undef S
    default: return 0;
  }
  if (iter->minor.dx) {
    iter->minor.c=w-1;
    iter->major.c=h-1;
  } else {
    iter->minor.c=h-1;
    iter->major.c=w-1;
  }
  iter->minor.d=image->stride*iter->minor.dy+iter->minor.dx*bytesperpixel;
  iter->major.d=image->stride*iter->major.dy+iter->major.dx*bytesperpixel;
  
  iter->x=x;
  iter->y=y;
  iter->pixelsize=bytesperpixel<<3;
  iter->minorc0=iter->minor.c;
  
  iter->minor.p=(char*)image->v+y*image->stride+x*bytesperpixel;
  iter->major.p=iter->minor.p;
  
  return 1;
}
