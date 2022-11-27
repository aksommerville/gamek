/* gamek_inmgr.h
 * Abstract input manager.
 * All PC platforms should use this.
 * Embedded platforms with a fixed set of inputs, probably not.
 */
 
#ifndef GAMEK_INMGR_H
#define GAMEK_INMGR_H

#include <stdint.h>

struct gamek_inmgr;

#define GAMEK_ACTION_QUIT          1
#define GAMEK_ACTION_FULLSCREEN    2 /* toggle */

/* Global inmgr context.
 **********************************************************************/

struct gamek_inmgr_delegate {
  void *userdata;
  int has_keyboard;
  int has_joysticks;
  int has_midi;
  
  /* Callbacks may fire during iteration or other state-dependent circumstances.
   * In general you don't need to worry about that, it's inmgr's problem.
   * However it is important that you not send new events while processing a callback.
   * I don't expect that to be a problem.
   * But do be mindful of it if you get input from background threads. Locking is YOUR problem, not inmgr's.
   */
  void (*button)(uint8_t playerid,uint16_t btnid,uint8_t value,void *userdata);
  void (*action)(uint16_t actionid,void *userdata);
  void (*midi)(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b,void *userdata);
  
  /* Called after we generate a new template.
   * You should save the file, if there is one.
   */
  void (*config_dirty)(struct gamek_inmgr *inmgr,void *userdata);
};

void gamek_inmgr_del(struct gamek_inmgr *inmgr);

struct gamek_inmgr *gamek_inmgr_new(const struct gamek_inmgr_delegate *delegate);

int gamek_inmgr_configure_text(struct gamek_inmgr *inmgr,const char *src,int srcc,const char *refname);
int gamek_inmgr_configure_file(struct gamek_inmgr *inmgr,const char *path);
int gamek_inmgr_encode_configuration(void *dstpp,struct gamek_inmgr *inmgr);
int gamek_inmgr_save_configuration(struct gamek_inmgr *inmgr,const char *path);

// Call when you're done adding configuration.
int gamek_inmgr_ready(struct gamek_inmgr *inmgr);

/* Joystick (or general input) events.
 ********************************************************************/

struct gamek_inmgr_joystick_capability {
  int btnid;
  int usage; // 32-bit HID usage if known
  int lo,hi; // declared value range
  int value; // initial or default value
};

struct gamek_inmgr_joystick_greeting {
  const void *id; // Some identifier from the supplier. Opaque to us.
  const char *name;
  int vid,pid;
  struct gamek_inmgr_joystick_capability *capv;
  int capc,capa;
};

void gamek_inmgr_joystick_greeting_cleanup(struct gamek_inmgr_joystick_greeting *greeting);

int gamek_inmgr_joystick_greeting_add_capability(
  struct gamek_inmgr_joystick_greeting *greeting,
  int btnid,int usage,int lo,int hi,int value
);

// Call when you're done adding capabilities, to sort them.
void gamek_inmgr_joystick_greeting_ready(struct gamek_inmgr_joystick_greeting *greeting);

// Valid only after 'ready':
struct gamek_inmgr_joystick_capability *gamek_inmgr_joystick_greeting_get_capability(
  const struct gamek_inmgr_joystick_greeting *greeting,
  int btnid
);
struct gamek_inmgr_joystick_capability *gamek_inmgr_joystick_greeting_get_capability_by_usage(
  const struct gamek_inmgr_joystick_greeting *greeting,
  int usage
);

int gamek_inmgr_joystick_connect(struct gamek_inmgr *inmgr,const struct gamek_inmgr_joystick_greeting *greeting);
int gamek_inmgr_joystick_disconnect(struct gamek_inmgr *inmgr,const void *id);
int gamek_inmgr_joystick_event(struct gamek_inmgr *inmgr,const void *id,int btnid,int value);

/* Keyboard and MIDI events.
 *******************************************************************/

// Nonzero if recognized.
int gamek_inmgr_keyboard_event(struct gamek_inmgr *inmgr,int keycode,int value);

/* We expect connect and disconnect events in the convention established by ossmidi:
 *   Connect: [0xf0,0xf7]
 *   Disconnect: []
 */
int gamek_inmgr_midi_events(struct gamek_inmgr *inmgr,int devid,const void *v,int c);

#endif
