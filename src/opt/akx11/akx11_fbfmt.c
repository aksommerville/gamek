#include "akx11_internal.h"

/* Measure stride.
 */
 
int akx11_fbfmt_measure_stride(int fbfmt,int w) {
  if (w<1) return 0;
  if (w>INT_MAX>>2) return 0; // sanity limit, regardless of pixel size
  switch (fbfmt) {
    case AKX11_FBFMT_XRGB:
    case AKX11_FBFMT_XBGR:
    case AKX11_FBFMT_RGBX:
    case AKX11_FBFMT_BGRX:
      return w<<2;
    case AKX11_FBFMT_BGR565LE:
    case AKX11_FBFMT_BGRX4444BE:
      return w<<1;
    case AKX11_FBFMT_BGR332:
    case AKX11_FBFMT_Y8:
      return w;
    case AKX11_FBFMT_Y1:
      return (w+7)>>3;
  }
  return 0;
}

/* Measure buffer.
 */
 
int akx11_fbfmt_measure_buffer(int fbfmt,int w,int h) {
  if (h<1) return 0;
  int stride=akx11_fbfmt_measure_stride(fbfmt,w);
  if (stride<1) return 0;
  if (stride>INT_MAX/h) return 0;
  return stride*h;
}

/* Convert among 32-bit RGB formats.
 */
 
void akx11_fbcvt_rgb(void *dst,uint8_t drs,uint8_t dgs,uint8_t dbs,const void *src,uint8_t srs,uint8_t sgs,uint8_t sbs,int w,int h) {
  uint32_t *dstp=dst;
  const uint32_t *srcp=src;
  int pxc=w*h;
  for (;pxc-->0;dstp++,srcp++) {
    uint8_t r=(*srcp)>>srs;
    uint8_t g=(*srcp)>>sgs;
    uint8_t b=(*srcp)>>sbs;
    *dstp=(r<<drs)|(g<<dgs)|(b<<dbs);
  }
}

/* Other framebuffer conversions, to serial RGBX.
 */
 
void akx11_fbcvt_rgb_bgr565le(void *dst,const void *src,int w,int h) {
  uint8_t *dstp=dst;
  const uint8_t *srcp=src;
  int pxc=w*h;
  for (;pxc-->0;dstp+=4,srcp+=2) {
    uint8_t r=(srcp[0]&0x1f)<<3; r|=r>>5;
    uint8_t g=((srcp[0]&0xe0)>>3)|((srcp[1]&0x07)<<5); g|=g>>6;
    uint8_t b=srcp[1]&0xf8; b|=b>>5;
    dstp[0]=r;
    dstp[1]=g;
    dstp[2]=b;
  }
}

void akx11_fbcvt_rgb_bgrx4444be(void *dst,const void *src,int w,int h) {
  uint8_t *dstp=dst;
  const uint8_t *srcp=src;
  int pxc=w*h;
  for (;pxc-->0;dstp+=4,srcp+=2) {
    uint8_t r=srcp[1]&0xf0; r|=r>>4;
    uint8_t g=srcp[0]&0x0f; g|=g<<4;
    uint8_t b=srcp[0]&0xf0; b|=b>>4;
    dstp[0]=r;
    dstp[1]=g;
    dstp[2]=b;
  }
}

void akx11_fbcvt_rgb_bgr332(void *dst,const void *src,int w,int h) {
  uint8_t *dstp=dst;
  const uint8_t *srcp=src;
  int pxc=w*h;
  for (;pxc-->0;dstp+=4,srcp++) {
    uint8_t r=(*srcp)&0x03; r|=r<<2; r|=r<<4;
    uint8_t g=((*srcp)&0x1c)<<3; g|=g>>3; g|=g>>6;
    uint8_t b=(*srcp)&0xe0; b|=b>>3; b|=b>>6;
    dstp[0]=r;
    dstp[1]=g;
    dstp[2]=b;
  }
}

void akx11_fbcvt_rgb_y1(void *dst,const void *src,int w,int h) {
  // y1 rows don't necessarily reach the right edge, so unlike other formats, we need to operate rowwise.
  uint32_t *dstrow=dst;
  const uint8_t *srcrow=src;
  int srcstride=(w+7)>>3;
  for (;h-->0;dstrow+=w,srcrow+=srcstride) {
    uint8_t *dstp=(uint8_t*)dstrow;
    const uint8_t *srcp=srcrow;
    uint8_t mask=0x80;
    int xi=w;
    for (;xi-->0;dstp+=4) {
      if ((*srcp)&mask) dstp[0]=dstp[1]=dstp[2]=0xff;
      else dstp[0]=dstp[1]=dstp[2]=0x00;
      if (mask) mask>>=1;
      else { srcp++; mask=0x80; }
    }
  }
}

void akx11_fbcvt_rgb_y8(void *dst,const void *src,int w,int h) {
  uint8_t *dstp=dst;
  const uint8_t *srcp=src;
  int pxc=w*h;
  for (;pxc-->0;dstp+=4,srcp++) {
    dstp[0]=*srcp;
    dstp[1]=*srcp;
    dstp[2]=*srcp;
  }
}

/* Scale one buffer to another, assuming the same 32-bit format.
 */

void akx11_scale_same32(void *dst,const void *src,int w,int h,int scale) {
  if (scale==1) {
    memcpy(dst,src,w*h*4);
    return;
  }
  uint32_t *dstrow=dst;
  const uint32_t *srcrow=src;
  int dststride=w*scale;
  int cprowc=scale-1;
  int cpc=dststride<<2;
  for (;h-->0;srcrow+=w) {
  
    uint32_t *dstp=dstrow;
    const uint32_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;srcp++) {
      int si=scale;
      for (;si-->0;dstp++) *dstp=*srcp;
    }
    dstp=dstrow;
    dstrow+=dststride;
    
    int ri=cprowc;
    for (;ri-->0;dstrow+=dststride) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

/* Convert from one 32-bit RGB format to another, while scaling up.
 */
 
void akx11_scale_swizzle(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs,uint8_t srs,uint8_t sgs,uint8_t sbs) {
  uint32_t *dstrow=dst;
  const uint32_t *srcrow=src;
  int dststride=w*scale;
  int cprowc=scale-1;
  int cpc=dststride<<2;
  for (;h-->0;srcrow+=w) {
  
    uint32_t *dstp=dstrow;
    const uint32_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;srcp++) {
      uint8_t r=(*srcp)>>srs;
      uint8_t g=(*srcp)>>sgs;
      uint8_t b=(*srcp)>>sbs;
      uint32_t pixel=(r<<drs)|(g<<dgs)|(b<<dbs);
      int si=scale;
      for (;si-->0;dstp++) *dstp=pixel;
    }
    dstp=dstrow;
    dstrow+=dststride;
    
    int ri=cprowc;
    for (;ri-->0;dstrow+=dststride) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

/* Other scale-up conversions.
 */
 
void akx11_scale_bgr565le(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs) {
  uint32_t *dstrow=dst;
  const uint8_t *srcrow=src;
  int srcstride=w<<1;
  int dststride=w*scale;
  int cprowc=scale-1;
  int cpc=dststride<<2;
  for (;h-->0;srcrow+=srcstride) {
  
    uint32_t *dstp=dstrow;
    const uint8_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;srcp+=2) {
      uint8_t r=(srcp[0]&0x1f)<<3; r|=r>>5;
      uint8_t g=((srcp[0]&0xe0)>>3)|((srcp[1]&0x07)<<5); g|=g>>6;
      uint8_t b=srcp[1]&0xf8; b|=b>>5;
      uint32_t pixel=(r<<drs)|(g<<dgs)|(b<<dbs);
      int si=scale;
      for (;si-->0;dstp++) *dstp=pixel;
    }
    dstp=dstrow;
    dstrow+=dststride;
    
    int ri=cprowc;
    for (;ri-->0;dstrow+=dststride) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

void akx11_scale_bgrx4444be(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs) {
  uint32_t *dstrow=dst;
  const uint8_t *srcrow=src;
  int srcstride=w<<1;
  int dststride=w*scale;
  int cprowc=scale-1;
  int cpc=dststride<<2;
  for (;h-->0;srcrow+=srcstride) {
  
    uint32_t *dstp=dstrow;
    const uint8_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;srcp+=2) {
      uint8_t r=srcp[1]&0xf0; r|=r>>4;
      uint8_t g=srcp[0]&0x0f; g|=g<<4;
      uint8_t b=srcp[0]&0xf0; b|=b>>4;
      uint32_t pixel=(r<<drs)|(g<<dgs)|(b<<dbs);
      int si=scale;
      for (;si-->0;dstp++) *dstp=pixel;
    }
    dstp=dstrow;
    dstrow+=dststride;
    
    int ri=cprowc;
    for (;ri-->0;dstrow+=dststride) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

void akx11_scale_bgr332(void *dst,const void *src,int w,int h,int scale,uint8_t drs,uint8_t dgs,uint8_t dbs) {
  uint32_t *dstrow=dst;
  const uint8_t *srcrow=src;
  int dststride=w*scale;
  int cprowc=scale-1;
  int cpc=dststride<<2;
  for (;h-->0;srcrow+=w) {
  
    uint32_t *dstp=dstrow;
    const uint8_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;srcp++) {
      uint8_t r=(*srcp)&0x03; r|=r<<2; r|=r<<4;
      uint8_t g=((*srcp)&0x1c)<<3; g|=g>>3; g|=g>>6;
      uint8_t b=(*srcp)&0xe0; b|=b>>3; b|=b>>6;
      uint32_t pixel=(r<<drs)|(g<<dgs)|(b<<dbs);
      int si=scale;
      for (;si-->0;dstp++) *dstp=pixel;
    }
    dstp=dstrow;
    dstrow+=dststride;
    
    int ri=cprowc;
    for (;ri-->0;dstrow+=dststride) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

void akx11_scale_y1(void *dst,const void *src,int w,int h,int scale) {
  uint32_t *dstrow=dst;
  const uint8_t *srcrow=src;
  int srcstride=(w+7)>>3;
  int dststride=w*scale;
  int cprowc=scale-1;
  int cpc=dststride<<2;
  for (;h-->0;srcrow+=srcstride) {
  
    uint32_t *dstp=dstrow;
    const uint8_t *srcp=srcrow;
    uint8_t mask=0x80;
    int xi=w;
    for (;xi-->0;) {
      uint32_t pixel=((*srcp)&mask)?0xffffffff:0;
      if (mask) mask>>=1;
      else { mask=0x80; srcp++; }
      int si=scale;
      for (;si-->0;dstp++) *dstp=pixel;
    }
    dstp=dstrow;
    dstrow+=dststride;
    
    int ri=cprowc;
    for (;ri-->0;dstrow+=dststride) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

void akx11_scale_y8(void *dst,const void *src,int w,int h,int scale) {
  uint32_t *dstrow=dst;
  const uint8_t *srcrow=src;
  int dststride=w*scale;
  int cprowc=scale-1;
  int cpc=dststride<<2;
  for (;h-->0;srcrow+=w) {
  
    uint32_t *dstp=dstrow;
    const uint8_t *srcp=srcrow;
    int xi=w;
    for (;xi-->0;srcp++) {
      uint32_t pixel=*srcp;
      pixel|=pixel<<8;
      pixel|=pixel<<16;
      int si=scale;
      for (;si-->0;dstp++) *dstp=pixel;
    }
    dstp=dstrow;
    dstrow+=dststride;
    
    int ri=cprowc;
    for (;ri-->0;dstrow+=dststride) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

/* Video mode descriptions.
 */
 
int akx11_video_mode_is_gx(int video_mode) {
  switch (video_mode) {
    case AKX11_VIDEO_MODE_OPENGL:
      return 1;
  }
  return 0;
}

int akx11_video_mode_is_fb(int video_mode) {
  switch (video_mode) {
    case AKX11_VIDEO_MODE_FB_GX:
    case AKX11_VIDEO_MODE_FB_PURE:
      return 1;
  }
  return 0;
}
