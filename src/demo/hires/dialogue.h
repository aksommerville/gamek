/* dialogue.h
 * Helper for hires demo, to generate word bubbles.
 */
 
#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <stdint.h>

struct gamek_image;

void dialogue_del(struct gamek_image *image);

struct gamek_image *dialogue_new(
  int16_t *dx,int16_t herox,int16_t worldw,
  const struct gamek_image *graphics,
  const char *src,int srcc
);

#endif
