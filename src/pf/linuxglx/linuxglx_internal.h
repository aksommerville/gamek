#ifndef LINUXGLX_INTERNAL_H
#define LINUXGLX_INTERNAL_H

#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "opt/inmgr/gamek_inmgr.h"
#include "opt/evdev/evdev.h"
#include "opt/ossmidi/ossmidi.h"
#include "opt/alsapcm/alsapcm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <limits.h>
#include <unistd.h>
#include <sys/poll.h>

#if GAMEK_USE_akx11
  #include "opt/akx11/akx11.h"
#endif
#if GAMEK_USE_akdrm
  #include "opt/akdrm/akdrm.h"
#endif

#define LINUXGLX_UPDATE_RATE_HZ 60
#define LINUXGLX_SLEEP_LIMIT_US 20000 /* Each frame should be 16666, anything larger is an error somehow. */

extern struct linuxglx {

  // Process stuff:
  const char *exename;
  volatile int terminate;
  int status;
  int use_stdin; // Look for action input from stdin. Only when DRM in play.
  
  // Configuration:
  int init_fullscreen;
  char *input_cfg_path;
  int audio_rate;
  int audio_chanc;
  char *audio_device;
  int audio_buffer_size;
  
  // Performance monitor:
  int uframec,vframec,aframec;
  double starttime_real;
  double starttime_cpu;
  int clockfaultc;
  
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
  int64_t nexttime; // us
  int64_t frametime; // us
  
  // Pixels pointer gets replaced each frame by whatever akx11 provides.
  struct gamek_image fb;
  
  // API-specific drivers:
  #if GAMEK_USE_akx11
    struct akx11 *akx11;
  #endif
  #if GAMEK_USE_akdrm
    struct akdrm *akdrm;
  #endif
  struct evdev *evdev;
  struct ossmidi *ossmidi;
  struct alsapcm *alsapcm;
  struct gamek_inmgr *inmgr;
  //TODO synthesizer
  
} linuxglx;

void linuxglx_perfmon_begin();
void linuxglx_perfmon_end();
double linuxglx_now_s();
double linuxglx_now_cpu_s();
int64_t linuxglx_now_us();

int linuxglx_io_update();

int linuxglx_video_init();
int linuxglx_video_update();
void linuxglx_video_render();

int linuxglx_input_init();

int linuxglx_audio_init();

#endif
