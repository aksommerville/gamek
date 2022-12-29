#ifndef GAMEK_MACOS_INTERNAL_H
#define GAMEK_MACOS_INTERNAL_H

/* We depend on four standalone opt units: macioc, macwm, macaudio, machid.
 * One could argue that these ought to live right here in pf/macos.
 * I made them separate, in case one or another needs replaced for some future Mac-ish platform.
 */
#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "opt/macwm/macwm.h"
#include "opt/macaudio/macaudio.h"
#include "opt/machid/machid.h"
#include "opt/inmgr/gamek_inmgr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//TODO higher-quality synthesizer
#if GAMEK_USE_mynth
  #include "opt/mynth/mynth.h"
#endif

#define GAMEK_MACOS_UPDATE_RATE_HZ 60

extern struct gamek_macos {

  // We yoink (argc,argv) at main() but then defer processing until macioc's init callback.
  // This gives macioc an opportunity to blank args that it uses (--reopen-tty,--chdir).
  const char *exename;
  int argc;
  char **argv;
  char *fs_sandbox;
  char *input_cfg_path;
  int init_fullscreen;
  int audio_rate;
  int audio_chanc;

  struct macwm *macwm;
  struct macaudio *macaudio;
  struct machid *machid;

  struct gamek_inmgr *inmgr;

  struct gamek_image fb;
  int audio_lock;

} gamek_macos;

// We implement a sticky lock. The unlocking is done at main update, lockers don't need to bother.
int gamek_macos_lock_audio();
void gamek_macos_unlock_audio();

#endif
