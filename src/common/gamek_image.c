#include "gamek_image.h"
#include "pf/gamek_pf.h" /* we don't use this; but tiny needs it, to acquire BYTE_ORDER */

#ifndef BYTE_ORDER
  #if GAMEK_USE_macos
    #include <machine/endian.h>
  #elif GAMEK_USE_evdev
    #include <endian.h>
  #else
    // Assume little-endian
    #define BIG_ENDIAN 4321
    #define LITTLE_ENDIAN 1234
    #define BYTE_ORDER LITTLE_ENDIAN
  #endif
#endif

/* Image format properties.
 */
 
uint32_t gamek_image_pixel_from_rgba(uint8_t fmt,uint8_t r,uint8_t g,uint8_t b,uint8_t a) {
  switch (fmt) {
    #if BYTE_ORDER==BIG_ENDIAN
      case GAMEK_IMGFMT_RGBX: return (r<<24)|(g<<16)|(b<<8)|a;
    #else
      case GAMEK_IMGFMT_RGBX: return r|(g<<8)|(b<<16)|(a<<24);
    #endif
    case GAMEK_IMGFMT_BGR332: return (b&0xe0)|((g>>3)&0x1c)|(r>>6);
  }
  return 0;
}

uint8_t gamek_image_format_pixel_size_bits(uint8_t fmt) {
  switch (fmt) {
    case GAMEK_IMGFMT_RGBX:
      return 32;
    case GAMEK_IMGFMT_BGR332:
      return 8;
  }
  return 0;
}

/* Decode serial image.
 */

int8_t gamek_image_decode(struct gamek_image *image,const void *src,int32_t srcc) {
  if (!image||!src||(srcc<6)) return -1;
  const uint8_t *SRC=src;
  
  image->fmt=SRC[0];
  image->flags=SRC[1]&~(GAMEK_IMAGE_FLAG_WRITEABLE|GAMEK_IMAGE_FLAG_OWNV);
  image->w=(SRC[2]<<8)|SRC[3];
  image->h=(SRC[4]<<8)|SRC[5];
  if (image->w<1) return -1;
  if (image->h<1) return -1;
  
  uint8_t pixelsize;
  switch (image->fmt) {
    case GAMEK_IMGFMT_RGBX: pixelsize=32; break;
    case GAMEK_IMGFMT_BGR332: pixelsize=8; break;
    default: return -1;
  }
  image->stride=(image->w*pixelsize+7)>>3;
  if (image->stride<1) return -1;
  if (image->stride>0x7fffffff/image->h) return -1;
  int32_t size=6+image->stride*image->h;
  if (size>srcc) return -1;
  
  image->v=(char*)src+6;
  
  return 0;
}
