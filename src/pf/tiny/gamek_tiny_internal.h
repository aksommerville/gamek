#ifndef GAMEK_TINY_INTERNAL_H
#define GAMEK_TINY_INTERNAL_H

#include "pf/gamek_pf.h"
#include "opt/mynth/mynth.h"
#include "common/gamek_image.h"

#define GAMEK_TINY_FBW 96
#define GAMEK_TINY_FBH 64
#define GAMEK_TINY_BYTES_PER_PIXEL 1
#define GAMEK_TINY_AUDIO_RATE 22050

extern struct gamek_tiny {
  uint8_t terminate;
  uint16_t input;
  struct gamek_image fb;
  uint8_t fb_storage[GAMEK_TINY_FBW*GAMEK_TINY_FBH*GAMEK_TINY_BYTES_PER_PIXEL];
} gamek_tiny;

void tiny_audio_init();

void tiny_video_init();
void tiny_video_swap(const uint8_t *fb);

#endif
