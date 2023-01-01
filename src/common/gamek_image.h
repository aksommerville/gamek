/* gamek_image.h
 */
 
#ifndef GAMEK_IMAGE_H
#define GAMEK_IMAGE_H

#include <stdint.h>

#define GAMEK_IMGFMT_RGBX 1 /* Serial order, regardless of host byte order. */
#define GAMEK_IMGFMT_BGR332 2

#define GAMEK_FOR_EACH_IMGFMT \
  _(RGBX) \
  _(BGR332)

#define GAMEK_IMAGE_FLAG_WRITEABLE    0x01
#define GAMEK_IMAGE_FLAG_OWNV         0x02
#define GAMEK_IMAGE_FLAG_TRANSPARENT  0x04

#define GAMEK_XFORM_XREV 1
#define GAMEK_XFORM_YREV 2
#define GAMEK_XFORM_SWAP 4

struct gamek_image {
  void *v;
  int16_t w,h;
  int16_t stride;
  uint8_t fmt;
  uint8_t flags;
};

/* Wrap an encoded image, as produced by our 'mkdata' tool, in a gamek_image.
 * (srcc) is optional. We will not read beyond that, but images are supposed to be self-terminating,
 * so as long as you trust the input, you can use (srcc==0x7fffffff).
 * The image we produce is WEAK, it points into the provided serial data and is not WRITEABLE.
 *
 * Serial format:
 *   1 fmt
 *   1 flags (WRITEABLE and OWNV are ignored, both always zero)
 *   2 w
 *   2 h
 *   ... pixels. Must use minimum stride.
 */
int8_t gamek_image_decode(struct gamek_image *image,const void *src,int32_t srcc);

/* Rendering.
 **************************************************************/

void gamek_image_clear(struct gamek_image *image);

void gamek_image_fill_rect(
  struct gamek_image *image,
  int16_t x,int16_t y,int16_t w,int16_t h,
  uint32_t pixel
);

void gamek_image_blit(
  struct gamek_image *dst,int16_t dstx,int16_t dsty,
  const struct gamek_image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,uint8_t xform
);

/* Convenience to render a square tile from a tilesheet up to 16x16 cells.
 * (dstx,dsty) is the center of the output.
 * (tilesize) is the width and height of a single tile.
 * (tileid) reads LRTB, 16 tiles per row (regardless of the image's actual size).
 * So tile (0,0) is 0x00, (1,0) is 0x01, (0,1) is 0x10, (15,15) is 0xff, etc.
 */
void gamek_image_blit_tile(
  struct gamek_image *dst,int16_t dstx,int16_t dsty,
  const struct gamek_image *src,int16_t tilesize,
  uint8_t tileid,uint8_t xform
);

/* A special kind of blitting, from a 1-bit source with no more than 32 pixels.
 * The set bits in source get the new color, zeroes are transparent.
 * Pixels in (src) are packed big-endianly, 0x80000000 is the top left corner.
 * Intended mostly for text.
 */
void gamek_image_render_glyph(
  struct gamek_image *image,int16_t dstx,int16_t dsty,
  uint32_t src,uint8_t w,uint8_t h,
  uint32_t pixel
);

uint32_t gamek_image_pixel_from_rgba(uint8_t fmt,uint8_t r,uint8_t g,uint8_t b,uint8_t a); // implies transparent destination
uint32_t gamek_image_pixel_from_rgb(uint8_t fmt,uint8_t r,uint8_t g,uint8_t b); // implies opaque destination
uint8_t gamek_image_format_pixel_size_bits(uint8_t fmt);

/* Iterator.
 * These read images bytewise.
 * TODO Are we going to need sub-byte pixels? Probably. That will take some more work here.
 ***************************************************************/
 
struct gamek_image_iterator_1d {
  void *p;
  int16_t d;
  int16_t c;
  int16_t dx,dy;
};
 
struct gamek_image_iterator {
  struct gamek_image_iterator_1d major;
  struct gamek_image_iterator_1d minor;
  int16_t minorc0;
  uint8_t pixelsize;
  int16_t x,y;
};

uint8_t gamek_image_iterator_init(
  struct gamek_image_iterator *iter,
  struct gamek_image *image,
  int16_t x,int16_t y,int16_t w,int16_t h,
  uint8_t xform
);

static inline uint32_t gamek_image_iterator_read(const struct gamek_image_iterator *iter) {
  if (iter->pixelsize==8) return *(uint8_t*)iter->minor.p;
  if (iter->pixelsize==16) return *(uint16_t*)iter->minor.p;
  if (iter->pixelsize==32) return *(uint32_t*)iter->minor.p;
  return 0;
}

static inline void gamek_image_iterator_write(struct gamek_image_iterator *iter,uint32_t pixel) {
  if (iter->pixelsize==8) *(uint8_t*)iter->minor.p=pixel;
  else if (iter->pixelsize==16) *(uint16_t*)iter->minor.p=pixel;
  else if (iter->pixelsize==32) *(uint32_t*)iter->minor.p=pixel;
}

static inline uint8_t gamek_image_iterator_next(struct gamek_image_iterator *iter) {
  if (iter->minor.c) {
    iter->minor.c--;
    iter->minor.p=(uint8_t*)iter->minor.p+iter->minor.d;
    iter->x+=iter->minor.dx;
    iter->y+=iter->minor.dy;
    return 1;
  }
  if (iter->major.c) {
    iter->major.c--;
    iter->major.p=(uint8_t*)iter->major.p+iter->major.d;
    iter->minor.p=iter->major.p;
    iter->minor.c=iter->minorc0;
    iter->x-=iter->minor.dx*iter->minor.c;
    iter->y-=iter->minor.dy*iter->minor.c;
    iter->x+=iter->major.dx;
    iter->y+=iter->major.dy;
    return 1;
  }
  return 0;
}

#endif
