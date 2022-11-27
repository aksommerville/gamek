#include "linuxglx_internal.h"
#include "opt/argv/gamek_argv.h"
#include <signal.h>

struct linuxglx linuxglx={0};

/* Cleanup.
 */
 
static void linuxglx_cleanup() {
  alsapcm_del(linuxglx.alsapcm);
  akx11_del(linuxglx.akx11);
  evdev_del(linuxglx.evdev);
  ossmidi_del(linuxglx.ossmidi);
  gamek_inmgr_del(linuxglx.inmgr);
  if (linuxglx.input_cfg_path) free(linuxglx.input_cfg_path);
}

/* Signals.
 */
 
static void _cb_signal(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(linuxglx.terminate)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",linuxglx.exename);
        exit(1);
      } break;
  }
}

/* --help
 */
 
static void linuxglx_print_help(const char *topic,int topicc) {
  fprintf(stderr,"Usage: %s [OPTIONS]\n",linuxglx.exename);
  fprintf(stderr,
    "OPTIONS:\n"
    "  --help            Print this message.\n"
    "  --fullscreen=0|1  Start in fullscreen.\n"
    "  --input=PATH      Path to input config (read/write).\n"
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
    linuxglx_print_help(v,vc);
    return 1; // >0 to terminate successfully
  }
  
  if ((kc==10)&&!memcmp(k,"fullscreen",10)) {
    linuxglx.init_fullscreen=vn;
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"input",5)) {
    if (linuxglx.input_cfg_path) free(linuxglx.input_cfg_path);
    if (!(linuxglx.input_cfg_path=malloc(vc+1))) return -1;
    memcpy(linuxglx.input_cfg_path,v,vc);
    linuxglx.input_cfg_path[vc]=0;
    return 0;
  }
  
  fprintf(stderr,
    "%s:WARNING: Unexpected argument '%.*s'='%.*s'\n",
    linuxglx.exename,kc,k,vc,v
  );
  return 0;
}

/* Update.
 */
 
static void linuxglx_update() {

  /* Evdev and MIDI use a sensible poller for updates.
   * ALSA uses a background thread, and X11 does its own thing.
   * If we add networking or... what... async file reading? That would be general "io" too.
   */
  if (linuxglx_io_update()<0) {
    linuxglx.terminate++;
    return;
  }
  
  /* X11 has its own update mechanism, it doesn't participate in poll.
   * There is Xlib API to expose file descriptors, but I've tried and never got it working.
   * I assume that's because it uses shared memory or something under the hood.
   */
  if (linuxglx_video_update()<0) {
    linuxglx.terminate++;
    return;
  }
  
  // Update game. Keep logical updates and video updates in sync, no reason not to.
  if (gamek_client.update) {
    gamek_client.update();
    linuxglx.uframec++;
  }
  if (gamek_client.render) {
    if (!(linuxglx.fb.v=akx11_begin_fb(linuxglx.akx11))) {
      linuxglx.terminate++;
      return;
    }
    gamek_client.render(&linuxglx.fb);
    akx11_end_fb(linuxglx.akx11,linuxglx.fb.v);
    linuxglx.vframec++;
  }
}

/* Main.
 */
 
int main(int argc,char **argv) {
  
  linuxglx.exename=gamek_argv_exename(argc,argv);
  int err=gamek_argv_read(argc,argv,_cb_argv,0);
  if (err<0) return 1;
  if (err>0) return 0;
  
  signal(SIGINT,_cb_signal);
  if (linuxglx_video_init()<0) return 1;
  if (linuxglx_input_init()<0) return 1;
  if (linuxglx_audio_init()<0) return 1;
  
  if (gamek_client.init) {
    if (gamek_client.init()<0) return 1;
  }
  
  // Start audio after client init, no sooner.
  if (linuxglx.alsapcm) {
    alsapcm_set_running(linuxglx.alsapcm,1);
  }
  
  linuxglx_perfmon_begin();
  linuxglx.nexttime=linuxglx_now_us();
  linuxglx.frametime=1000000/LINUXGLX_UPDATE_RATE_HZ;
  
  while (!linuxglx.terminate) {
    linuxglx_update();
  }
  
  if (!linuxglx.status) {
    linuxglx_perfmon_end();
  }
  
  linuxglx_cleanup();
  
  return linuxglx.status;
}
