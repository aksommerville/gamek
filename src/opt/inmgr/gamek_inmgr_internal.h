#ifndef GAMEK_INMGR_INTERNAL_H
#define GAMEK_INMGR_INTERNAL_H

#include "gamek_inmgr.h"
#include "pf/gamek_pf.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#define GAMEK_BUTTON_HAT (GAMEK_BUTTON_LEFT|GAMEK_BUTTON_RIGHT|GAMEK_BUTTON_UP|GAMEK_BUTTON_DOWN)
#define GAMEK_BUTTON_HORZ (GAMEK_BUTTON_LEFT|GAMEK_BUTTON_RIGHT)
#define GAMEK_BUTTON_VERT (GAMEK_BUTTON_UP|GAMEK_BUTTON_DOWN)

// Live mapping for one button.
struct gamek_inmgr_map {
  const void *id; // Zero for system keyboard, not addressable via joystick events.
  int srcbtnid;
  
  /* Rotational hats are special. Use a single map with (srchi==srclo+7) and (dstbtnid==UP|DOWN|LEFT|RIGHT).
   * (srclo) is the minimum value, which corresponds to UP. The next 7 values proceed clockwise.
   * For all other button types, (srclo..srchi) inclusive is ON.
   */
  int srclo,srchi;
  
  int srcvalue;
  uint8_t dstvalue;
  uint8_t playerid; // Zero for actions (dstbtnid=actionid). Player zero is special anyway.
  uint16_t dstbtnid; // Single bit, actionid, or hat indicator.
};

#define GAMEK_INMGR_TM_BTN_FLAG_ACTION      0x01 /* otherwise it's a button or hat */
#define GAMEK_INMGR_TM_BTN_FLAG_LOWPRIORITY 0x02 /* don't consume a playerid, next device gets the same. for keyboard */
#define GAMEK_INMGR_TM_BTN_FLAG_PLAYER2     0x04 /* use 2 playerids from one device. but 2's the limit */
#define GAMEK_INMGR_TM_BTN_FLAG_USAGE       0x08 /* srcbtnid is USB-HID usage, not the hardware btnid */
#define GAMEK_INMGR_TM_BTN_FLAG_CRITICAL    0x10 /* reject device match if this button is absent */

struct gamek_inmgr_tm_btn {
  int srcbtnid;
  int srclo,srchi;
  uint8_t flags;
  uint16_t dstbtnid;
};

struct gamek_inmgr_tm {
  uint16_t vid,pid; // zero to match all
  char *name; // substring of device name. empty matches all. (case-sensitive, but we force everything lower)
  struct gamek_inmgr_tm_btn *btnv;
  int btnc,btna;
};

struct gamek_inmgr_midi {
  int devid;
  uint8_t chid; // 0xff to use incoming channels, 0..15 to override all.
  uint8_t status,a,b;
};

struct gamek_inmgr {
  struct gamek_inmgr_delegate delegate;
  int ready;
  
  /* Live maps are listed per button, all devices combined in one list.
   * Not sure whether it makes sense to structure this in two levels, device then button?
   * This way seems simpler, so going with it.
   * Sort by (id,srcbtnid). Duplicates are legal.
   */
  struct gamek_inmgr_map *mapv;
  int mapc,mapa;
  
  uint16_t state_by_playerid[256];
  uint8_t playerid_next;
  uint8_t playerid_max;
  
  /* Templates, from config file.
   * Order matters: We use the first match.
   */
  struct gamek_inmgr_tm *tmv;
  int tmc,tma;
  
  struct gamek_inmgr_midi *midiv;
  int midic,midia;
};

/* There can be more than one match per button.
 * This returns the lowest matching index, or -insp-1.
 */
int gamek_inmgr_mapv_search(const struct gamek_inmgr *inmgr,const void *id,int srcbtnid);

/* (c) is usually 1, but you can insert multiple records at once.
 * eg if you're mapping an axis to two buttons.
 * ^ I ended up not needing this behavior, but it remains.
 */
struct gamek_inmgr_map *gamek_inmgr_mapv_insert(struct gamek_inmgr *inmgr,int p,const void *id,int srcbtnid,int c);

// Same as gamek_inmgr_joystick_connect() but permits id zero. We use this to connect the system keyboard.
int gamek_inmgr_connect_private(struct gamek_inmgr *inmgr,const struct gamek_inmgr_joystick_greeting *greeting);

void gamek_inmgr_tm_cleanup(struct gamek_inmgr_tm *tm);

struct gamek_inmgr_tm *gamek_inmgr_tm_new(
  struct gamek_inmgr *inmgr,
  const char *name,int namec,
  uint16_t vid,uint16_t pid
);
struct gamek_inmgr_tm_btn *gamek_inmgr_tm_btnv_append(struct gamek_inmgr_tm *tm);

int gamek_inmgr_tm_match(
  const struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_greeting *greeting
);

/* If we're able to, create a template for this device, add it to inmgr's list, and return it.
 * We won't create templates for devices that don't look useful.
 */
struct gamek_inmgr_tm *gamek_inmgr_synthesize_template(
  struct gamek_inmgr *inmgr,
  const struct gamek_inmgr_joystick_greeting *greeting
);

/* Create live maps for one device, from its greeting and a matched template.
 */
int gamek_inmgr_map_device(
  struct gamek_inmgr *inmgr,
  const struct gamek_inmgr_joystick_greeting *greeting,
  const struct gamek_inmgr_tm *tm
);

void gamek_inmgr_send_button(struct gamek_inmgr *inmgr,uint8_t playerid,uint16_t btnid,uint8_t value);

#endif
