#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/gamek_image.h"
#include "opt/png/png.h"

/* Rewrite pixels from standard RGBA to Gamek RGBX.
 * Just means if alpha is zero, make the whole pixel zero.
 */
 
static void mkdata_png_to_rgbx(struct png_image *image) {
  uint8_t *row=image->pixels;
  int yi=image->h;
  for (;yi-->0;row+=image->stride) {
    uint8_t *p=row;
    int xi=image->w;
    for (;xi-->0;p+=4) {
      if (!p[3]) {
        p[0]=p[1]=p[2]=0;
      }
    }
  }
}

/* Rewrite pixels from standard RGBA to BGR332.
 * This will reduce the image's stride.
 * Natural zeroes in the output are transparent. Maybe we'll change that eventually to pure green or something?
 */
 
static void mkdata_png_to_bgr332(struct png_image *image) {
  int srcstride=image->stride;
  int dststride=image->w;
  uint8_t *srcrow=image->pixels;
  uint8_t *dstrow=image->pixels;
  int yi=image->h;
  for (;yi-->0;srcrow+=srcstride,dstrow+=dststride) {
    uint8_t *srcp=srcrow;
    uint8_t *dstp=dstrow;
    int xi=image->w;
    for (;xi-->0;srcp+=4,dstp+=1) {
      if (srcp[3]) {
        uint8_t r=srcp[0],g=srcp[1],b=srcp[2];
        uint8_t bgr=(b&0xe0)|((g>>3)&0x1c)|(r>>6);
        if (!bgr) bgr=0x20; // substitute darkest blue for black; black means transparent
        *dstp=bgr;
      } else {
        *dstp=0;
      }
    }
  }
  image->stride=dststride;
}

/* Convert to whatever pixel format was requested.
 * May replace the image object and free the old one.
 */
 
static int mkdata_png_convert(struct png_image **image,const char *path,int imgfmt) {
  
  /* First use png's own facilities to convert to a standard input format.
   * Usually that means RGBA, regardless of what we're outputting.
   * But if we're outputting gray, we can get that here too.
   * Also if we ever need cropping or padding, that can happen in the same pass.
   */
  uint8_t depth=(*image)->depth;
  uint8_t colortype=(*image)->colortype;
  switch (imgfmt) {
    case GAMEK_IMGFMT_BGR332:
    case GAMEK_IMGFMT_RGBX: {
        png_depth_colortype_8bit(&depth,&colortype);
        png_depth_colortype_rgb(&depth,&colortype);
        png_depth_colortype_alpha(&depth,&colortype);
      } break;
    default: {
        fprintf(stderr,"%s: Unknown imgfmt %d at %s:%d\n",path,imgfmt,__FILE__,__LINE__);
        return -2;
      }
  }
  struct png_image *converted=png_image_reformat(*image,0,0,0,0,depth,colortype,0);
  if (!converted) {
    fprintf(stderr,"%s: png_image_reformat failed\n",path);
    return -1;
  }
  png_image_del(*image);
  *image=converted;
  
  /* Now convert from the standardized image to the real output format.
   * For now, outputs are always equal or smaller than inputs, so we never reallocate.
   * When this process succeeds, (pixels,stride) are correct but don't necessarily agree with (depth,colortype).
   */
  switch (imgfmt) {
    case GAMEK_IMGFMT_RGBX: mkdata_png_to_rgbx(*image); break;
    case GAMEK_IMGFMT_BGR332: mkdata_png_to_bgr332(*image); break;
  }
  
  return 0;
}

/* Gamek image from PNG, main entry point.
 */
 
int mkdata_image_from_png(void *dstpp,const void *src,int srcc,const char *path,int imgfmt) {
  
  // Decode PNG and convert to the requested format.
  struct png_image *png=png_decode(src,srcc);
  if (!png) {
    fprintf(stderr,"%s: Failed to decode as PNG\n",path);
    return -2;
  }
  if ((png->w>0x7fff)||(png->h>0x7fff)) {
    fprintf(stderr,"%s: Too big. %dx%d, limit 32767.\n",path,png->w,png->h);
    png_image_del(png);
    return -2;
  }
  int transparent=0;
  if ((png->colortype==4)||(png->colortype==6)) transparent=1;
  int err=mkdata_png_convert(&png,path,imgfmt);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to convert image.\n",path);
    png_image_del(png);
    return -1;
  }
  
  // Final size is now known: stride*h + 6 byte header.
  int dstc=6+png->stride*png->h;
  uint8_t *dst=malloc(dstc);
  if (!dst) {
    png_image_del(png);
    return -1;
  }
  
  dst[0]=imgfmt;
  dst[1]=
    (transparent?GAMEK_IMAGE_FLAG_TRANSPARENT:0)|
  0;
  dst[2]=png->w>>8;
  dst[3]=png->w;
  dst[4]=png->h>>8;
  dst[5]=png->h;
  memcpy(dst+6,png->pixels,png->stride*png->h);
  
  png_image_del(png);
  *(void**)dstpp=dst;
  return dstc;
}
