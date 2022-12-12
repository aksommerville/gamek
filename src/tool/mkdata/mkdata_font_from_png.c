#include <stdio.h>
#include <stdlib.h>
#include "opt/png/png.h"

/* Nonzero if at least one pixel in this raw y8 is opaque (>=0x80).
 */
 
static int mkdata_rect_contains_opaque(const uint8_t *src,int stride,int w,int h) {
  for (;h-->0;src+=stride) {
    const uint8_t *p=src;
    int xi=w;
    for (;xi-->0;p++) if (*p>=0x80) return 1;
  }
  return 0;
}

/* Read one glyph from raw y8.
 */
 
static int mkdata_read_glyph(uint32_t *glyph,const uint8_t *src,int stride,int w,int h,const char *path,int codepoint) {
  int descent=0;

  // Bring in all four edges until it's empty or the edges all have something on them.
  // The top edge is meaningful and can't shift more than 3 rows.
  while ((w>0)&&(h>0)) {
    if ((descent<3)&&!mkdata_rect_contains_opaque(src,stride,w,1)) { // top
      descent++;
      src+=stride;
      h--;
    } else if (!mkdata_rect_contains_opaque(src,stride,1,h)) { // left
      src++;
      w--;
    } else if (!mkdata_rect_contains_opaque(src+(h-1)*stride,stride,w,1)) { // bottom
      h--;
    } else if (!mkdata_rect_contains_opaque(src+w-1,stride,1,h)) { // right
      w--;
    } else {
      break;
    }
  }
  // Zero is a valid, empty glyph.
  // But when we find an empty slot, let's assume it's SPACE, and give it a width of 4 pixels.
  // Note that it's OR not AND; one axis would collapse before the other.
  if (!w||!h) { *glyph=0x20000000; return 0; } // descent=0, w=4, content all zeroes
  
  // Width must be in (1..7).
  if (w<1) return 0;
  if (w>7) {
    fprintf(stderr,"%s:%02x:WARNING: Clamping glyph width from %d to 7\n",path,codepoint,w);
    w=7;
  }
  
  // Produce the 5-bit header from (descent) and (w).
  uint32_t tmp=(descent<<30)|(w<<27);
  
  // Read pixels.
  uint32_t mask=1<<26;
  for (;h-->0;src+=stride) {
    const uint8_t *p=src;
    int xi=w;
    for (;xi-->0;p++,mask>>=1) {
      if (*p>=0x80) {
        if (!mask) fprintf(stderr,"%s:%02x:WARNING: Extra glyph bit after the 27th\n",path,codepoint);
        tmp|=mask;
      }
    }
  }
  
  *glyph=tmp;
  return 0;
}

/* Height of encoded glyph, including descent.
 */
 
static int mkdata_glyph_height(uint32_t glyph) {
  int w=(glyph>>27)&7;
  if (!w) return 0;
  int descent=(glyph>>30);
  uint32_t rowmask=(1<<w)-1;
  rowmask<<=27-w; // start at the top...
  int h=0,y=0;
  for (;rowmask;rowmask>>=w,y++) {
    if (glyph&rowmask) h=y+1;
  }
  return descent+h;
}

/* Read up to 96 glyphs off a y8 image, and calculate line height.
 */
 
static int mkdata_read_font_image(
  uint32_t *glyphv/*96*/,
  const struct png_image *image,
  int colw,int rowh,
  int colc,int rowc,
  int codepoint,
  const char *path
) {
  if (colc*rowc>96) return -1;
  if ((image->w<colw*colc)||(image->h<rowh*rowc)) return -1;
  if ((image->depth!=8)||(image->colortype!=0)) return -1;
  int lineheight=0;
  int ribbonstride=image->stride*rowh;
  const uint8_t *row=image->pixels;
  int yi=rowc; for (;yi-->0;row+=ribbonstride) {
    const uint8_t *col=row;
    int xi=colc; for (;xi-->0;col+=colw,glyphv++,codepoint++) {
      int err=mkdata_read_glyph(glyphv,col,image->stride,colw,rowh,path,codepoint);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s:%02x: Failed to read glyph.\n",path,codepoint);
        return -2;
      }
      int h1=mkdata_glyph_height(*glyphv);
      if (h1>lineheight) lineheight=h1;
    }
  }
  return lineheight+1;
}

/* Based on the cell counts, selected the base codepoint.
 * For now we are supporting 16x6=>0x20 and 10x1=>0x30.
 */
 
static int mkdata_font_choose_codepoint_from_dimensions(int colc,int rowc) {
  if ((colc==16)&&(rowc==6)) return 0x20;
  if ((colc==10)&&(rowc==1)) return 0x30;
  return -1;
}

/* Font from PNG, main entry point.
 */
 
int mkdata_font_from_png(void *dstpp,const void *src,int srcc,const char *path) {

  /* Decode, validate dimensions, and convert to 8-bit luma.
   */
  struct png_image *image=png_decode(src,srcc);
  if (!image) {
    fprintf(stderr,"%s: Failed to decode PNG file.\n",path);
    return -2;
  }
  const int colw=8,rowh=12;
  int colc=image->w/colw;
  int rowc=image->h/rowh;
  int codepoint0=mkdata_font_choose_codepoint_from_dimensions(colc,rowc);
  if ((codepoint0<0)||(image->w%colw)||(image->h%rowh)) {
    fprintf(stderr,
      "%s: Found %dx%d image, expected %dx%d or %dx%d (16x6 or 10x1 glyphs of %dx%d pixels)\n",
      path,image->w,image->h,colw*16,rowh*6,colw*10,rowh*1,colw,rowh
    );
    png_image_del(image);
    return -2;
  }
  uint8_t depth=image->depth,colortype=image->colortype;
  png_depth_colortype_8bit(&depth,&colortype);
  png_depth_colortype_luma(&depth,&colortype);
  png_depth_colortype_opaque(&depth,&colortype);
  {
    struct png_image *converted=png_image_reformat(image,0,0,0,0,depth,colortype,0);
    png_image_del(image);
    if (!converted) {
      fprintf(stderr,"%s: Failed to convert image.\n",path);
      return -2;
    }
    image=converted;
  }
  
  /* Convert image into glyphs and line height.
   */
  uint32_t glyphv[96];
  int lineheight=mkdata_read_font_image(glyphv,image,colw,rowh,colc,rowc,codepoint0,path);
  if (lineheight<1) {
    if (lineheight!=-2) fprintf(stderr,"%s: Failed to read glyphs from image.\n",path);
    png_image_del(image);
    return -2;
  }
  png_image_del(image);
  
  // And finally, encode the font:
  int glyphc=colc*rowc;
  int dstc=
    1+ // lineheight
    2+ // table header
    glyphc*4+ // glyphs
    1; // terminator
  uint8_t *dst=malloc(dstc);
  if (!dst) return -1;
  
  dst[0]=lineheight;
  dst[1]=glyphc; // glyph count, table 0
  dst[2]=codepoint0; // first code point, table 0
  {
    uint8_t *dstp=dst+3;
    const uint32_t *srcp=glyphv;
    int i=glyphc; for (;i-->0;dstp+=4,srcp++) {
      dstp[0]=(*srcp)>>24;
      dstp[1]=(*srcp)>>16;
      dstp[2]=(*srcp)>>8;
      dstp[3]=(*srcp);
    }
  }
  dst[dstc-1]=0; // terminator
  
  *(void**)dstpp=dst;
  return dstc;
}
