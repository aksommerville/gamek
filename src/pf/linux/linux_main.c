#include "linux_internal.h"
#include "opt/argv/gamek_argv.h"
#include <signal.h>
#include <time.h>

struct gamek_linux gamek_linux={0};

/* Cleanup.
 */
 
static void linux_cleanup() {
  alsapcm_del(gamek_linux.alsapcm);
  #if GAMEK_USE_akx11
    akx11_del(gamek_linux.akx11);
  #endif
  #if GAMEK_USE_akdrm
    akdrm_del(gamek_linux.akdrm);
  #endif
  evdev_del(gamek_linux.evdev);
  ossmidi_del(gamek_linux.ossmidi);
  gamek_inmgr_del(gamek_linux.inmgr);
  if (gamek_linux.input_cfg_path) free(gamek_linux.input_cfg_path);
  if (gamek_linux.audio_device) free(gamek_linux.audio_device);
  if (gamek_linux.fs_sandbox) free(gamek_linux.fs_sandbox);
}

/* Signals.
 */
 
static void _cb_signal(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(gamek_linux.terminate)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",gamek_linux.exename);
        exit(1);
      } break;
  }
}

/* --help
 */
 
static void linux_print_help(const char *topic,int topicc) {
  fprintf(stderr,"Usage: %s [OPTIONS]\n",gamek_linux.exename);
  fprintf(stderr,
    "\n"
    "OPTIONS:\n"
    "  --help                  Print this message.\n"
    "  --fullscreen=0|1        Start in fullscreen.\n"
    "  --input=PATH            Path to input config (read/write).\n"
    "  --audio-rate=HZ         Output rate.\n"
    "  --audio-chanc=1|2       Channel count.\n"
    "  --audio-device=NAME     eg 'pcmC0D0p', see /dev/snd.\n"
    "  --audio-buffer=INT      Audio buffer size in frames.\n"
    "  --fs-sandbox=PATH       Directory accessible to the game.\n"
    "\n"
    "DEFAULT KEYBOARD MAPPING:\n"
    "  ESC      Quit\n"
    "  F11      Toggle fullscreen\n"
    "  Arrows   P1 Dpad\n"
    "  Z,X,A,S  P1 thumb buttons\n"
    "  Enter    P1 start\n"
    "  Tab      P1 select\n"
    "  Space    P1 aux 3\n"
    "  Grave,BS P1 trigger buttons\n"
    "  1,Equal  P2 secondary triggers\n"
  );
}

/* Argv.
 */
 
static int _cb_argv(const char *k,int kc,const char *v,int vc,int vn,void *userdata) {
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    linux_print_help(v,vc);
    return 1; // >0 to terminate successfully
  }
  
  if ((kc==10)&&!memcmp(k,"fullscreen",10)) {
    gamek_linux.init_fullscreen=vn;
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"input",5)) {
    if (gamek_linux.input_cfg_path) free(gamek_linux.input_cfg_path);
    if (!(gamek_linux.input_cfg_path=malloc(vc+1))) return -1;
    memcpy(gamek_linux.input_cfg_path,v,vc);
    gamek_linux.input_cfg_path[vc]=0;
    return 0;
  }
  
  if ((kc==10)&&!memcmp(k,"audio-rate",10)) {
    gamek_linux.audio_rate=vn;
    return 0;
  }
  
  if ((kc==11)&&!memcmp(k,"audio-chanc",11)) {
    gamek_linux.audio_chanc=vn;
    return 0;
  }
  
  if ((kc==12)&&!memcmp(k,"audio-buffer",12)) {
    gamek_linux.audio_buffer_size=vn;
    return 0;
  }
  
  if ((kc==12)&&!memcmp(k,"audio-device",12)) {
    if (gamek_linux.audio_device) free(gamek_linux.audio_device);
    if (!(gamek_linux.audio_device=malloc(vc+1))) return -1;
    memcpy(gamek_linux.audio_device,v,vc);
    gamek_linux.audio_device[vc]=0;
    return 0;
  }
  
  if ((kc==10)&&!memcmp(k,"fs-sandbox",10)) {
    if (gamek_linux.fs_sandbox) free(gamek_linux.fs_sandbox);
    if (vc) {
      if (!(gamek_linux.fs_sandbox=malloc(vc+1))) return -1;
      memcpy(gamek_linux.fs_sandbox,v,vc);
      gamek_linux.fs_sandbox[vc]=0;
    } else {
      gamek_linux.fs_sandbox=0;
    }
    return 0;
  }
  
  fprintf(stderr,
    "%s:WARNING: Unexpected argument '%.*s'='%.*s'\n",
    gamek_linux.exename,kc,k,vc,v
  );
  return 0;
}

/* Update.
 */
 
static void linux_update() {

  /* Evdev and MIDI use a sensible poller for updates.
   * ALSA uses a background thread, and X11 does its own thing.
   * If we add networking or... what... async file reading? That would be general "io" too.
   */
  if (linux_io_update()<0) {
    gamek_linux.terminate++;
    return;
  }
  
  /* X11 has its own update mechanism, it doesn't participate in poll.
   * There is Xlib API to expose file descriptors, but I've tried and never got it working.
   * I assume that's because it uses shared memory or something under the hood.
   */
  if (linux_video_update()<0) {
    gamek_linux.terminate++;
    return;
  }
  
  // Update game. Keep logical updates and video updates in sync, no reason not to.
  if (gamek_client.update) {
    gamek_client.update();
    gamek_linux.uframec++;
  }
  if (gamek_client.render) {
    linux_video_render();
    gamek_linux.vframec++;
  }
}

/* Terminate per client request.
 */
 
void gamek_platform_terminate(uint8_t status) {
  fprintf(stderr,"Client requests termination with status %d\n",status);
  gamek_linux.terminate++;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  
  gamek_linux.exename=gamek_argv_exename(argc,argv);
  int err=gamek_argv_read(argc,argv,_cb_argv,0);
  if (err<0) return 1;
  if (err>0) return 0;
  
  signal(SIGINT,_cb_signal);
  if (linux_video_init()<0) return 1;
  if (linux_input_init()<0) return 1;
  if (linux_audio_init()<0) return 1;
  
  //TODO Should this be configurable somehow?
  srand(time(0));
  
  if (gamek_client.init) {
    if (gamek_client.init()<0) return 1;
  }
  
  // Start audio after client init, no sooner.
  if (gamek_linux.alsapcm) {
    alsapcm_set_running(gamek_linux.alsapcm,1);
  }
  
  // Input delivery was suspended at init. OK to begin it now.
  gamek_inmgr_enable(gamek_linux.inmgr,1);
  
  linux_perfmon_begin();
  gamek_linux.nexttime=linux_now_us();
  gamek_linux.frametime=1000000/LINUX_UPDATE_RATE_HZ;
  
  while (!gamek_linux.terminate) {
    linux_update();
  }
  
  if (!gamek_linux.status) {
    linux_perfmon_end();
  }
  
  linux_cleanup();
  
  return gamek_linux.status;
}

/* Platform definition.
 */
 
const struct gamek_platform_details gamek_platform_details={
  .name="linux",
  .capabilities=
    GAMEK_PLATFORM_CAPABILITY_TERMINATE|
    GAMEK_PLATFORM_CAPABILITY_KEYBOARD|
    GAMEK_PLATFORM_CAPABILITY_MOUSE|
    GAMEK_PLATFORM_CAPABILITY_PLUG_INPUT|
    GAMEK_PLATFORM_CAPABILITY_MULTIPLAYER|
    GAMEK_PLATFORM_CAPABILITY_NETWORK|
    GAMEK_PLATFORM_CAPABILITY_AUDIO|
    GAMEK_PLATFORM_CAPABILITY_MIDI|
    GAMEK_PLATFORM_CAPABILITY_LANGUAGE|
    GAMEK_PLATFORM_CAPABILITY_STORAGE|
  0,
};
