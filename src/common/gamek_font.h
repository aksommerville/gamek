/* gamek_font.h
 * We provide helpers for text rendering, a very restrictive regime.
 * A font may contain up to 256 glyphs.
 * All text is treated as ISO 8859-1, ie single bytes of Unicode.
 *
 * Each glyph is stored in 32 bits.
 *   2 descent: 0..3
 *   3 width: 0..7
 *   28 content
 *
 * The font encodes to a self-terminated array of bytes:
 *   1 line height
 *   ... glyph tables:
 *     1 glyph count, >0
 *     1 first code point -- must be in order
 *     ... glyphs:
 *       1 0xc0=descent, 0x38=width, 0x07=first 3 pixels
 *       3 remaining pixels
 *   1 terminator: 0
 *
 * A typical font, containing a single table of G0, takes 388 bytes.
 */
 
#ifndef GAMEK_FONT_H
#define GAMEK_FONT_H

#include <stdint.h>

struct gamek_image;

int16_t gamek_font_render_string(
  struct gamek_image *dst,
  int16_t x,int16_t y,
  uint32_t color,
  const void *font,
  const char *src,int16_t srcc
);

int16_t gamek_font_render_glyph(
  struct gamek_image *dst,
  int16_t x,int16_t y,
  uint32_t color,
  const void *font,
  char codepoint
);

const void *gamek_font_find_glyph_table(
  const void *font,
  char codepoint
);

uint32_t gamek_font_find_glyph_in_table(
  const void *table,
  char codepoint
);

#endif
