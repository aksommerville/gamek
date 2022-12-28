#ifndef FIDDLE_INTERNAL_H
#define FIDDLE_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include "opt/fs/gamek_fs.h"

// MIDI-In API.
#if GAMEK_USE_alsamidi
  #include "opt/alsamidi/alsamidi.h"
#endif

// PCM-Out API.
#if GAMEK_USE_alsapcm
  #include "opt/alsapcm/alsapcm.h"
#endif

// Synthesizer.
#if GAMEK_USE_mynth
  #include "opt/mynth/mynth.h"
#endif

#ifndef FIDDLE_USE_INOTIFY
  #define FIDDLE_USE_INOTIFY 1
#endif
#if FIDDLE_USE_INOTIFY
  #include <sys/inotify.h>
#endif

#define FIDDLE_WAVE_COUNT 8
#define FIDDLE_WAVE_SIZE 512 /* samples */

extern struct fiddle {
  volatile int terminate;
  int lockc;
  int rate,chanc;
  
  #if GAMEK_USE_alsamidi
    struct alsamidi *alsamidi;
  #endif
  
  #if GAMEK_USE_alsapcm
    struct alsapcm *alsapcm;
  #endif
  
  #if GAMEK_USE_mynth
    // mynth context is global
  #endif
  
  #if FIDDLE_USE_INOTIFY
    int infd;
    struct fiddle_watchdir {
      char *dir;
      int dirc;
      int wd;
    } *watchdirv;
    int watchdirc,watchdira;
    struct fiddle_watchfile {
      int waveid;
      char *base;
      int basec;
      int wd;
    } *watchfilev;
    int watchfilec,watchfilea;
  #endif
  
  int16_t waves[FIDDLE_WAVE_COUNT*FIDDLE_WAVE_SIZE];
  
  int ragec;
  
} fiddle;

int fiddle_lock();
void fiddle_unlock();

int fiddle_set_wave(int waveid,const char *path,int pathc);
int fiddle_waves_update();
int fiddle_waves_init();

// Emit some noise on the main out, to signal something went wrong,
// in case the user can't see his terminal, which is likely.
void fiddle_unleash_rage();

/* This one hook from mkdata actually lives in tool/common so we can use it too.
 */
int mkdata_wavebin_from_wave(void *dstpp,const void *src,int srcc,const char *path);

#endif
