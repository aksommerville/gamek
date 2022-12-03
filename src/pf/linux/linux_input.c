#include "linux_internal.h"

/* Device connected.
 */
 
static int _cb_evdev_cap(int type,int code,int usage,int lo,int hi,int value,void *userdata) {
  struct gamek_inmgr_joystick_greeting *greeting=userdata;
  gamek_inmgr_joystick_greeting_add_capability(
    greeting,(type<<16)|code,usage,lo,hi,value
  );
  return 0;
}
 
static void _cb_evdev_connect(struct evdev_device *device) {
  struct gamek_inmgr_joystick_greeting greeting={
    .id=device,
    .vid=evdev_device_get_vid(device),
    .pid=evdev_device_get_pid(device),
  };
  char name[256];
  int namec=evdev_device_get_name(name,sizeof(name),device);
  if ((namec<0)||(namec>=sizeof(name))) namec=0;
  if (namec) greeting.name=name;
  evdev_device_enumerate(device,_cb_evdev_cap,&greeting);
  gamek_inmgr_joystick_connect(gamek_linux.inmgr,&greeting);
  gamek_inmgr_joystick_greeting_cleanup(&greeting);
}

/* Provider callbacks trivially routed to inmgr.
 */
 
static void _cb_evdev_disconnect(struct evdev_device *device) {
  gamek_inmgr_joystick_disconnect(gamek_linux.inmgr,device);
}
 
static void _cb_evdev_button(struct evdev_device *device,int type,int code,int value) {
  gamek_inmgr_joystick_event(gamek_linux.inmgr,device,(type<<16)|code,value);
}
 
static void _cb_ossmidi_events(int devid,const void *v,int c,void *userdata) {
  gamek_inmgr_midi_events(gamek_linux.inmgr,devid,v,c);
}

/* Lost inotify.
 */
 
static void _cb_evdev_lost_inotify(struct evdev *evdev) {
  fprintf(stderr,"%s:WARNING: Lost evdev inotify. New joystick connections will not be detected.\n",gamek_linux.exename);
}

static void _cb_ossmidi_lost_inotify(void *userdata) {
  fprintf(stderr,"%s:WARNING: Lost ossmidi inotify. New MIDI connections will not be detected.\n",gamek_linux.exename);
}

/* Callbacks from inmgr, ready for delivery.
 */
 
static void _cb_inmgr_button(uint8_t playerid,uint16_t btnid,uint8_t value,void *userdata) {
  if (gamek_client.input_event) {
    gamek_client.input_event(playerid,btnid,value);
  }
}

static void _cb_inmgr_midi(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,void *userdata) {
  if (gamek_client.midi_in) {
    gamek_client.midi_in(chid,opcode,a,b);
  }
}

/* Enumerated stateless actions, from inmgr.
 */

static void _cb_inmgr_action(uint16_t action,void *userdata) {
  switch (action) {
    case GAMEK_ACTION_QUIT: gamek_linux.terminate++; break;
    case GAMEK_ACTION_FULLSCREEN: akx11_set_fullscreen(gamek_linux.akx11,akx11_get_fullscreen(gamek_linux.akx11)?0:1); break;
  }
}

/* Inmgr changed its configuration. We can rewrite the file.
 */
 
static void _cb_inmgr_config_dirty(struct gamek_inmgr *inmgr,void *userdata) {
  if (gamek_linux.input_cfg_path) {
    if (gamek_inmgr_save_configuration(inmgr,gamek_linux.input_cfg_path)<0) {
      fprintf(stderr,"%s:WARNING: Failed to save input config.\n",gamek_linux.input_cfg_path);
    }
  }
}

/* Init.
 */
 
int linux_input_init() {

  /* inmgr
   */
  struct gamek_inmgr_delegate idelegate={
    .has_keyboard=1,
    .has_joysticks=1,
    .has_midi=gamek_client.midi_in?1:0,
    .button=_cb_inmgr_button,
    .action=_cb_inmgr_action,
    .midi=_cb_inmgr_midi,
    .config_dirty=_cb_inmgr_config_dirty,
  };
  if (!(gamek_linux.inmgr=gamek_inmgr_new(&idelegate))) return -1;
  if (gamek_linux.input_cfg_path) {
    if (gamek_inmgr_configure_file(gamek_linux.inmgr,gamek_linux.input_cfg_path)<0) {
      fprintf(stderr,"%s:WARNING: Failed to apply input config.\n",gamek_linux.input_cfg_path);
    }
  }
  if (gamek_inmgr_ready(gamek_linux.inmgr)<0) return -1;

  /* evdev
   */
  struct evdev_delegate delegate={
    .connect=_cb_evdev_connect,
    .disconnect=_cb_evdev_disconnect,
    .button=_cb_evdev_button,
    .lost_inotify=_cb_evdev_lost_inotify,
  };
  if (!(gamek_linux.evdev=evdev_new(&delegate,0))) {
    fprintf(stderr,"%s: Failed to initialize evdev.\n",gamek_linux.exename);
    return -1;
  }
  if (evdev_scan_now(gamek_linux.evdev)<0) return -1;
  
  /* ossmidi
   * Works about the same as evdev, so I keep them together.
   */
  if (gamek_client.midi_in) {
    struct ossmidi_delegate mdelegate={
      .events=_cb_ossmidi_events,
      .lost_inotify=_cb_ossmidi_lost_inotify,
    };
    if (!(gamek_linux.ossmidi=ossmidi_new(&mdelegate))) {
      fprintf(stderr,"%s: Failed to initialize MIDI.\n",gamek_linux.exename);
      return -1;
    }
    if (ossmidi_rescan_now(gamek_linux.ossmidi)<0) return -1;
  }
  
  return 0;
}
