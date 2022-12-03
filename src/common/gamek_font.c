#include "gamek_font.h"
#include "gamek_image.h"
#include <stdio.h>

/* Render a string.
 */

int16_t gamek_font_render_string(
  struct gamek_image *dst,
  int16_t x,int16_t y,
  uint32_t color,
  const void *font,
  const char *src,int16_t srcc
) {
  if (!src||!font) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int16_t x0=x;
  const uint8_t *table=0;
  for (;srcc-->0;src++) {
    if (!table||((unsigned char)(*src)<table[1])||((unsigned char)(*src)>=table[0]+table[1])) {
      if (!(table=gamek_font_find_glyph_table(font,*src))) continue;
    }
    const uint8_t *v=table+2+(((unsigned char)(*src)-table[1])<<2);
    uint32_t glyph=(v[0]<<24)|(v[1]<<16)|(v[2]<<8)|v[3];
    if (!glyph) continue;
    
    uint8_t descent=glyph>>30;
    uint8_t w=(glyph>>27)&7;
    if (!w) continue;
    uint8_t h=27/w+1; // bump up by one instead of rounding up, it's easier
    
    gamek_image_render_glyph(dst,x,y+descent,glyph<<5,w,h,color);
    x+=w+1;
  }
  return x-x0;
}

/* Render one glyph from scratch.
 */

int16_t gamek_font_render_glyph(
  struct gamek_image *dst,
  int16_t x,int16_t y,
  uint32_t color,
  const void *font,
  char codepoint
) {
  const void *table=gamek_font_find_glyph_table(font,codepoint);
  if (!table) return 0;
  uint32_t glyph=gamek_font_find_glyph_in_table(table,codepoint);
  if (!glyph) return 0;
  y+=glyph>>30; // descent
  uint8_t w=(glyph>>27)&7;
  if (!w) return 0;
  uint8_t h=27/w+1; // bump up by one instead of rounding up, it's easier
  gamek_image_render_glyph(dst,x,y,glyph<<5,w,h,color);
  return w+1;
}

/* Find table in font.
 */

const void *gamek_font_find_glyph_table(
  const void *font,
  char codepoint
) {
  if (!font) return 0;
  const uint8_t *v=font;
  v+=1; // skip header
  while (*v) {
    if ((unsigned char)codepoint<v[1]) break;
    if ((unsigned char)codepoint<v[0]+v[1]) return v;
    v+=2+(v[0]<<2);
  }
  return 0;
}

/* Find glyph in table.
 */

uint32_t gamek_font_find_glyph_in_table(
  const void *table,
  char codepoint
) {
  if (!table) return 0;
  const uint8_t *TABLE=table;
  if ((unsigned char)codepoint<TABLE[1]) return 0;
  codepoint-=TABLE[1];
  if ((unsigned char)codepoint>=TABLE[0]) return 0;
  const uint8_t *v=TABLE+2+((unsigned char)codepoint<<2);
  return (v[0]<<24)|(v[1]<<16)|(v[2]<<8)|v[3];
}
