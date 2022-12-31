#include "gamek_macos_internal.h"
#include "opt/macioc/macioc.h"
#include "opt/argv/gamek_argv.h"

struct gamek_macos gamek_macos={0};

/* Audio lock.
 */
 
int gamek_macos_lock_audio() {
  if (gamek_macos.audio_lock) return 0;
  if (!gamek_macos.macaudio) return 0;
  if (macaudio_lock(gamek_macos.macaudio)<0) return -1;
  gamek_macos.audio_lock=1;
  return 0;
}

void gamek_macos_unlock_audio() {
  if (!gamek_macos.audio_lock) return;
  macaudio_unlock(gamek_macos.macaudio);
  gamek_macos.audio_lock=0;
}

/* Terminate per client.
 */

void gamek_platform_terminate(uint8_t status) {
  macioc_terminate(status);
}

/* Audio requests from client.
 */

void gamek_platform_audio_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  if (gamek_macos_lock_audio()<0) return;
  #if GAMEK_USE_mynth
    mynth_event(chid,opcode,a,b);
  #endif
}

void gamek_platform_play_song(const void *v,uint16_t c) {
  if (gamek_macos_lock_audio()<0) return;
  #if GAMEK_USE_mynth
    mynth_play_song(v,c);
  #endif
}

void gamek_platform_set_audio_wave(uint8_t waveid,const int16_t *v,uint16_t c) {
  if (gamek_macos_lock_audio()<0) return;
  #if GAMEK_USE_mynth
    if (c==512) {
      mynth_set_wave(waveid,v);
    }
  #endif
}

void gamek_platform_audio_configure(const void *v,uint16_t c) {
  // Mynth doesn't do this, but better synths probably will.
}

/* Input config dirty.
 */
  
static void gamek_macos_cb_inmgr_config_dirty(struct gamek_inmgr *inmgr,void *userdata) {
  if (!gamek_macos.input_cfg_path) return;
  if (gamek_inmgr_save_configuration(inmgr,gamek_macos.input_cfg_path)<0) {
    fprintf(stderr,"%s:WARNING: Failed to save input config.\n",gamek_macos.input_cfg_path);
  } else {
    fprintf(stderr,"%s: Saved input config\n",gamek_macos.input_cfg_path);
  }
}

/* Digested button state change.
 */

static void gamek_macos_cb_button(uint8_t playerid,uint16_t btnid,uint8_t value,void *userdata) {
  if (gamek_client.input_event) gamek_client.input_event(playerid,btnid,value);
}

/* Digested stateless action.
 */
 
static void gamek_macos_cb_action(uint16_t actionid,void *userdata) {
  switch (actionid) {
    case GAMEK_ACTION_QUIT: gamek_platform_terminate(0); break;
    case GAMEK_ACTION_FULLSCREEN: macwm_set_fullscreen(gamek_macos.macwm,MACWM_FULLSCREEN_TOGGLE); break;
  }
}

/* Digested MIDI event.
 */
 
static void gamek_macos_cb_midi(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,void *userdata) {
  if (gamek_client.midi_in) gamek_client.midi_in(chid,opcode,a,b);
}

/* Keyboard input.
 */

static int gamek_macos_cb_key(void *userdata,int keycode,int value) {
  gamek_inmgr_keyboard_event(gamek_macos.inmgr,keycode,value);
  return 1; // Nonzero to ack, so macwm won't attempt Unicode resolution.
}

/* Non-keyboard HID input.
 */

static int gamek_macos_cb_hid_cap(int btnid,int usage,int lo,int hi,int value,void *userdata) {
  if (gamek_inmgr_joystick_greeting_add_capability(userdata,btnid,usage,lo,hi,value)<0) return -1;
  return 0;
}

static void gamek_macos_cb_hid_connect(struct machid *machid,int devid) {
  struct gamek_inmgr_joystick_greeting greeting={
    .id=(void*)(uintptr_t)devid,
  };
  greeting.name=machid_get_ids(&greeting.vid,&greeting.pid,machid,devid);
  machid_enumerate(machid,devid,gamek_macos_cb_hid_cap,&greeting);
  gamek_inmgr_joystick_greeting_ready(&greeting);
  gamek_inmgr_joystick_connect(gamek_macos.inmgr,&greeting);
  gamek_inmgr_joystick_greeting_cleanup(&greeting);
}

static void gamek_macos_cb_hid_disconnect(struct machid *machid,int devid) {
  gamek_inmgr_joystick_disconnect(gamek_macos.inmgr,(void*)(uintptr_t)devid);
}

static void gamek_macos_cb_hid_button(struct machid *machid,int devid,int btnid,int value) {
  gamek_inmgr_joystick_event(gamek_macos.inmgr,(void*)(uintptr_t)devid,btnid,value);
}

/* MIDI in.
 */

static void gamek_macos_cb_midi_in(const void *src,int srcc,void *userdata) {
  /* !!! macaudio has all the hooks but does not actually implement MIDI-In yet.
   * We'll be ready for it, once I get around to that.
   */
  gamek_inmgr_midi_events(gamek_macos.inmgr,0,src,srcc);
}

/* Generate PCM.
 */

// Mono=>Stereo is a common enough case that it deserves special handling.
static void gamek_macos_expand_audio_stereo(int16_t *v,int framec) {
  int16_t *dst=v+(framec<<1);
  const int16_t *src=v+framec;
  while (framec-->0) {
    src--;
    dst-=2;
    dst[0]=dst[1]=*src;
  }
}

static void gamek_macos_expand_audio_multi(int16_t *v,int framec,int chanc) {
  int16_t *dst=v+framec*chanc;
  const int16_t *src=v+framec;
  while (framec-->0) {
    src--;
    int i=chanc;
    while (i-->0) {
      dst--;
      *dst=*src;
    }
  }
}

static void gamek_macos_cb_pcm_out(int16_t *v,int c,void *userdata) {
  #if GAMEK_USE_mynth
    int chanc=macaudio_get_chanc(gamek_macos.macaudio);
    if (chanc==1) {
      mynth_update(v,c);
    } else if (chanc==2) {
      int framec=c>>1;
      mynth_update(v,framec);
      gamek_macos_expand_audio_stereo(v,framec);
    } else {
      int framec=c/chanc;
      mynth_update(v,framec);
      gamek_macos_expand_audio_multi(v,framec,chanc);
    }
  #else
    memset(v,0,c<<1);
  #endif
}

/* Cleanup.
 */

static void gamek_macos_cb_quit(void *userdata) {

  //TODO performance report
  
  macwm_del(gamek_macos.macwm);
  macaudio_del(gamek_macos.macaudio);
  machid_del(gamek_macos.machid);

  gamek_inmgr_del(gamek_macos.inmgr);

  if (gamek_macos.fb.v) free(gamek_macos.fb.v);
  if (gamek_macos.fs_sandbox) free(gamek_macos.fs_sandbox);
  if (gamek_macos.input_cfg_path) free(gamek_macos.input_cfg_path);

  memset(&gamek_macos,0,sizeof(struct gamek_macos));
}

/* Init video.
 */

static int gamek_macos_video_init() {

  struct macwm_delegate delegate={
    .key=gamek_macos_cb_key,
    // 'close' not implemented because NSApplication will quit when the last (only) window closes, just as we would.
    // Not implemented because not needed: resize, text, mmotion, mbutton, mwheel
  };
  struct macwm_setup setup={
    .fullscreen=gamek_macos.init_fullscreen,
    .title=gamek_client.title,
    .rendermode=MACWM_RENDERMODE_FRAMEBUFFER,
    .fbw=gamek_client.fbw,
    .fbh=gamek_client.fbh,
  };
  if ((setup.fbw<1)||(setup.fbh<1)) {
    setup.fbw=640;
    setup.fbh=360;
  }
  if (!(gamek_macos.macwm=macwm_new(&delegate,&setup))) {
    fprintf(stderr,"Failed to initialize macwm\n");
    return -1;
  }

  gamek_macos.fb.w=setup.fbw;
  gamek_macos.fb.h=setup.fbh;
  gamek_macos.fb.stride=gamek_macos.fb.w<<2;
  gamek_macos.fb.fmt=GAMEK_IMGFMT_RGBX;
  gamek_macos.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;
  if (!(gamek_macos.fb.v=malloc(gamek_macos.fb.stride*gamek_macos.fb.h))) return -1;

  return 0;
}

/* Init audio.
 */

static int gamek_macos_audio_init() {

  struct macaudio_delegate delegate={
    .pcm_out=gamek_macos_cb_pcm_out,
    // 'midi_in' only if the client is going to use it.
  };
  if (gamek_client.midi_in) {
    delegate.midi_in=gamek_macos_cb_midi_in;
  }
  struct macaudio_setup setup={
    .rate=gamek_macos.audio_rate,
    .chanc=gamek_macos.audio_chanc,
  };
  if (setup.rate<1) setup.rate=44100;
  if (setup.chanc<1) setup.chanc=1;
  if (!(gamek_macos.macaudio=macaudio_new(&delegate,&setup))) {
    fprintf(stderr,"Failed to initialize macaudio\n");
    return -1;//TODO proceed without?
  }

  #if GAMEK_USE_mynth
    mynth_init(macaudio_get_rate(gamek_macos.macaudio));
  #endif

  macaudio_play(gamek_macos.macaudio,1);
  return 0;
}

/* Init input.
 */

static int gamek_macos_input_init() {

  struct machid_delegate delegate={
    .connect=gamek_macos_cb_hid_connect,
    .disconnect=gamek_macos_cb_hid_disconnect,
    .button=gamek_macos_cb_hid_button,
    // Not implemented: devid_next, filter
  };
  if (!(gamek_macos.machid=machid_new(&delegate))) {
    fprintf(stderr,"Failed to initialize machid\n");
    return -1;//TODO proceed without?
  }

  struct gamek_inmgr_delegate idelegate={
    .has_keyboard=1,
    .has_joysticks=1,
    .has_midi=1,
    .button=gamek_macos_cb_button,
    .action=gamek_macos_cb_action,
    .midi=gamek_macos_cb_midi,
    .config_dirty=gamek_macos_cb_inmgr_config_dirty,
  };
  if (!(gamek_macos.inmgr=gamek_inmgr_new(&idelegate))) return -1;
  gamek_inmgr_enable(gamek_macos.inmgr,0);
  if (gamek_macos.input_cfg_path) {
    if (gamek_inmgr_configure_file(gamek_macos.inmgr,gamek_macos.input_cfg_path)<0) {
      fprintf(stderr,"%s: Failed to apply input config.\n",gamek_macos.input_cfg_path);
    }
  }
  if (gamek_inmgr_ready(gamek_macos.inmgr)<0) return -1;
  
  return 0;
}

/* --help
 */

static void gamek_macos_print_help() {

  fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",gamek_macos.exename);
  int bundlepfxc=0,i=0;
  for (;gamek_macos.exename[i];i++) {
    if (gamek_macos.exename[i]=='/') {
      if ((i>4)&&!memcmp(gamek_macos.exename+i-4,".app",4)) {
        bundlepfxc=i;
        break;
      }
    }
  }
  if (bundlepfxc) {
    fprintf(stderr,"Or more Macfully: open -W %.*s --args --reopen-tty=$(tty) --chdir=$(pwd) [OPTIONS]\n\n",bundlepfxc,gamek_macos.exename);
  }

  fprintf(stderr,
    "OPTIONS:\n"
    "  --help                  Print this message.\n"
    "  --reopen-tty=DEVICE     Redirect stdin,stdout,stderr. MacOS stubs them by default.\n"
    "  --chdir=PATH            Change working directory after launch. MacOS sets to a sandbox by default.\n"
    "  --input=PATH            Path to input config (read/write).\n"
    "  --audio-rate=HZ         Output rate.\n"
    "  --audio-chanc=1|2       Channel count.\n"
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
    "\n"
  );
}

/* Command line.
 */

static int gamek_macos_cb_arg(
  const char *k,int kc,
  const char *v,int vc,int vn,
  void *userdata
) {

  if ((kc==4)&&!memcmp(k,"help",4)) {
    gamek_macos_print_help();
    return 1;
  }

  // --reopen-tty,--chdir managed by macioc already.
  
  if ((kc==10)&&!memcmp(k,"fullscreen",10)) {
    gamek_macos.init_fullscreen=vn;
    return 0;
  }
  
  if ((kc==5)&&!memcmp(k,"input",5)) {
    if (gamek_macos.input_cfg_path) free(gamek_macos.input_cfg_path);
    if (!(gamek_macos.input_cfg_path=malloc(vc+1))) return -1;
    memcpy(gamek_macos.input_cfg_path,v,vc);
    gamek_macos.input_cfg_path[vc]=0;
    return 0;
  }
  
  if ((kc==10)&&!memcmp(k,"audio-rate",10)) {
    gamek_macos.audio_rate=vn;
    return 0;
  }
  
  if ((kc==11)&&!memcmp(k,"audio-chanc",11)) {
    gamek_macos.audio_chanc=vn;
    return 0;
  }
  
  if ((kc==10)&&!memcmp(k,"fs-sandbox",10)) {
    if (gamek_macos.fs_sandbox) free(gamek_macos.fs_sandbox);
    if (vc) {
      if (!(gamek_macos.fs_sandbox=malloc(vc+1))) return -1;
      memcpy(gamek_macos.fs_sandbox,v,vc);
      gamek_macos.fs_sandbox[vc]=0;
    } else {
      gamek_macos.fs_sandbox=0;
    }
    return 0;
  }

  fprintf(stderr,"%s:WARNING: Unexpected option '%.*s'='%.*s'\n",gamek_macos.exename,kc,k,vc,v);
  return 0;
}

/* Initialize.
 */

static int gamek_macos_cb_init(void *userdata) {

  int err=gamek_argv_read(gamek_macos.argc,gamek_macos.argv,gamek_macos_cb_arg,0);
  if (err<0) return -1;
  if (err) {
    gamek_platform_terminate(0);
    return 0;
  }
  
  if (gamek_macos_video_init()<0) return -1;
  if (gamek_macos_audio_init()<0) return -1;
  if (gamek_macos_input_init()<0) return -1;

  if (gamek_client.init&&(gamek_client.init()<0)) {
    fprintf(stderr,"Failed to initialize client app\n");
    return -1;
  }
  
  gamek_inmgr_enable(gamek_macos.inmgr,1);
  
  return 0;
}

/* Update.
 */

static void gamek_macos_cb_update(void *userdata) {

  if (machid_update(gamek_macos.machid,0.010)<0) {
    fprintf(stderr,"Error updating machid\n");
    gamek_platform_terminate(1);
    return;
  }
  gamek_macos_unlock_audio(); // We might have locked during input processing, and might have slept a bit after.
  
  if (macwm_update(gamek_macos.macwm)<0) {
    fprintf(stderr,"Error updating macwm\n");
    gamek_platform_terminate(1);
    return;
  }

  if (gamek_client.update) gamek_client.update();
  gamek_macos_unlock_audio();

  if (gamek_client.render&&gamek_client.render(&gamek_macos.fb)) {
    macwm_send_framebuffer(gamek_macos.macwm,gamek_macos.fb.v);
  }
}

/* Main.
 */

int main(int argc,char **argv) {

  gamek_macos.exename=gamek_argv_exename(argc,argv);
  gamek_macos.argc=argc;
  gamek_macos.argv=argv;
  
  struct macioc_delegate delegate={
    .rate=GAMEK_MACOS_UPDATE_RATE_HZ,
    .quit=gamek_macos_cb_quit,
    .init=gamek_macos_cb_init,
    .update=gamek_macos_cb_update,
    // Not implemented: focus
  };
  return macioc_main(argc,argv,&delegate);
}

/* Platform definition.
 */

const struct gamek_platform_details gamek_platform_details={
  .name="macos",
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
