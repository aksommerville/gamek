#include "dialogue.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include <stdlib.h>
#include <stdio.h>

extern const uint8_t font_g06[];

/* Delete.
 */

void dialogue_del(struct gamek_image *image) {
  if (!image) return;
  if (image->v) free(image->v);
  free(image);
}

/* Measure text.
 */
 
static int16_t dialogue_measure_text(
  int16_t *rowv,int16_t rowa,
  int16_t *textw,int16_t *texth,int16_t *lineh,
  const char *src,int srcc,
  int16_t limitw
) {
  *textw=*texth=*lineh=0;
  
  // First select a constant line height. Our fonts have fixed line heights, it's the first byte.
  *lineh=font_g06[0];
  
  int16_t rowc=0;
  int srcp=0;
  while (srcp<srcc) {
  
    // Too many rows? Might as well stop.
    if (rowc>=rowa) break;
    rowv[rowc++]=srcp;
    
    // Read out to the next LF. We break there (inclusive) no matter what else.
    // If there isn't one, (nlp) will be the same as (srcc), which is great.
    int nlp=srcp;
    while ((nlp<srcc)&&(src[nlp++]!=0x0a)) {}
    
    // Line begins with whitespace+word+whitespace, even if it's too long.
    const char *line=src+srcp;
    int16_t linec=0;
    while ((srcp<nlp)&&((unsigned char)src[srcp]<=0x20)) { srcp++; linec++; }
    while ((srcp<nlp)&&((unsigned char)src[srcp]>0x20)) { srcp++; linec++; }
    while ((srcp<nlp)&&((unsigned char)src[srcp]<=0x20)) { srcp++; linec++; }
  
    // Measure what we have so far, then add word+whitespace until we exceed (limitw) or (nlp).
    int16_t linew=gamek_font_render_string(0,0,0,0,font_g06,line,linec);
    while ((linew<limitw)&&(srcp<nlp)) {
      const char *next=src+srcp;
      int16_t nextc=0;
      while ((srcp+nextc<nlp)&&((unsigned char)next[nextc]>0x20)) nextc++;
      while ((srcp+nextc<nlp)&&((unsigned char)next[nextc]<=0x20)) nextc++;
      int16_t nextw=gamek_font_render_string(0,0,0,0,font_g06,next,nextc);
      if (linew>limitw-nextw) break;
      linew+=nextw;
      linec+=nextc;
      srcp+=nextc;
    }
    
    // Update total dimensions.
    // For the horizontal, if there's a chance it's greater, first drop trailing whitespace, then measure again.
    if (linew>*textw) {
      while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
      linew=gamek_font_render_string(0,0,0,0,font_g06,line,linec);
      if (linew>*textw) *textw=linew;
    }
    (*texth)+=(*lineh);
  }
  return rowc;
}

/* New.
 */

struct gamek_image *dialogue_new(
  int16_t *dx,int16_t herox,int16_t worldw,
  const struct gamek_image *graphics,
  const char *src,int srcc
) {
  if (!graphics||!src||(srcc<1)) return 0;

  // Break lines and measure text.
  #define ROW_LIMIT 10
  int16_t limitw=worldw/5; // arbitrary width limit proportionate to framebuffer size
  int16_t rowv[ROW_LIMIT];
  int16_t textw=0,texth=0,lineheight=0;
  int16_t rowc=dialogue_measure_text(rowv,ROW_LIMIT,&textw,&texth,&lineheight,src,srcc,limitw);
  if (rowc<0) return 0;
  if (rowc>ROW_LIMIT) rowc=ROW_LIMIT;
  #undef ROW_LIMIT
  
  // Add fixed margins on each side to get the image size.
  // Image must be at least 8x14 to fit the frame.
  const int16_t marginl=4;
  const int16_t marginr=4;
  const int16_t margint=4;
  const int16_t marginb=11; // includes about 6 pixels for the arrow
  int16_t imgw=marginl+textw+marginr;
  int16_t imgh=margint+texth+marginb;
  if (imgw<8) imgw=8;
  if (imgh<14) imgh=14;
  
  // Allocate the image, using the same format as 'graphics'.
  struct gamek_image *image=calloc(1,sizeof(struct gamek_image));
  if (!image) return 0;
  image->w=imgw;
  image->h=imgh;
  image->fmt=graphics->fmt;
  image->flags=GAMEK_IMAGE_FLAG_TRANSPARENT|GAMEK_IMAGE_FLAG_OWNV|GAMEK_IMAGE_FLAG_WRITEABLE;
  image->stride=(gamek_image_format_pixel_size_bits(image->fmt)*image->w+7)>>3;
  if (!(image->v=calloc(image->stride,image->h))) {
    free(image);
    return 0;
  }
  
  // Render background frame.
  // We draw the corners and arrow verbatim. The bulk of the bubble is three colors that we select from the source bubble image.
  // Gamek, by design, does not have a "read_pixel" function. Use an iterator.
  uint32_t color_frame=0,color_shade=0,color_body=0;
  struct gamek_image_iterator iterator={0};
  if (gamek_image_iterator_init(&iterator,(struct gamek_image*)graphics,103,32,1,3,0)) {
    color_frame=gamek_image_iterator_read(&iterator); gamek_image_iterator_next(&iterator);
    color_shade=gamek_image_iterator_read(&iterator); gamek_image_iterator_next(&iterator);
    color_body=gamek_image_iterator_read(&iterator);
  }
  gamek_image_fill_rect(image,8,0,image->w-16,1,color_frame);
  gamek_image_fill_rect(image,8,image->h-7,image->w-16,1,color_frame);
  gamek_image_fill_rect(image,0,8,1,image->h-22,color_frame);
  gamek_image_fill_rect(image,image->w-1,8,1,image->h-22,color_frame);
  gamek_image_fill_rect(image,8,1,image->w-16,1,color_shade);
  gamek_image_fill_rect(image,8,image->h-8,image->w-16,1,color_shade);
  gamek_image_fill_rect(image,1,8,1,image->h-22,color_shade);
  gamek_image_fill_rect(image,image->w-2,8,1,image->h-22,color_shade);
  gamek_image_fill_rect(image,8,2,image->w-16,image->h-10,color_body);
  gamek_image_fill_rect(image,2,8,6,image->h-22,color_body);
  gamek_image_fill_rect(image,image->w-8,8,6,image->h-22,color_body);
  gamek_image_blit(image,0,0,graphics,96,32,8,8,0); // corners...
  gamek_image_blit(image,image->w-8,0,graphics,104,32,8,8,0);
  gamek_image_blit(image,0,image->h-14,graphics,96,40,8,8,0);
  gamek_image_blit(image,image->w-8,image->h-14,graphics,104,40,8,8,0);
  
  // Select arrow position and render it.
  // No need to get fancy here, just put it always in the center.
  *dx=-(image->w>>1);
  gamek_image_blit_tile(image,-*dx,image->h-6,graphics,16,0x27,0);
  
  // Render text.
  uint32_t textcolor=gamek_image_pixel_from_rgba(image->fmt,0x00,0x00,0x00,0xff);
  int16_t rowi=0;
  int16_t dsty=margint;
  for (;rowi<rowc;rowi++,dsty+=lineheight) {
    const char *rowsrc=src+rowv[rowi];
    int16_t rowsrcc;
    if (rowi<rowc-1) rowsrcc=rowv[rowi+1]-rowv[rowi];
    else rowsrcc=srcc-rowv[rowi];
    gamek_font_render_string(image,marginl,dsty,textcolor,font_g06,rowsrc,rowsrcc);
  }
  
  return image;
}
