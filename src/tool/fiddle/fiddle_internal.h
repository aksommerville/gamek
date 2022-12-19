#ifndef FIDDLE_INTERNAL_H
#define FIDDLE_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

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
  
} fiddle;

#endif
