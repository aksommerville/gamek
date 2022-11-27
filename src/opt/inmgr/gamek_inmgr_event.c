#include "gamek_inmgr_internal.h"

/* Consider button change and notify delegate accordingly.
 */
 
void gamek_inmgr_send_button(struct gamek_inmgr *inmgr,uint8_t playerid,uint16_t btnid,uint8_t value) {
  if (!playerid) return; // illegal to set player zero directly
  if (!btnid) return; // must be one bit; but we only assert nonzero
  
  if (value) {
    if (inmgr->state_by_playerid[playerid]&btnid) return;
    inmgr->state_by_playerid[playerid]|=btnid;
  } else {
    if (!(inmgr->state_by_playerid[playerid]&btnid)) return;
    inmgr->state_by_playerid[playerid]&=~btnid;
  }
  if (inmgr->delegate.button) inmgr->delegate.button(playerid,btnid,value,inmgr->delegate.userdata);
  
  // Now, if the player's own button changed, cascade it to the aggregate state.
  if (value) {
    if (inmgr->state_by_playerid[0]&btnid) return;
    inmgr->state_by_playerid[0]|=btnid;
  } else {
    if (!(inmgr->state_by_playerid[0]&btnid)) return;
    inmgr->state_by_playerid[0]&=~btnid;
  }
  if (inmgr->delegate.button) inmgr->delegate.button(0,btnid,value,inmgr->delegate.userdata);
}

/* Consider hat change and notify delegate accordingly.
 */

static void gamek_inmgr_send_hat(struct gamek_inmgr *inmgr,uint8_t playerid,uint8_t value) {
  if (!playerid) return;
  uint16_t newstate=0;
  switch (value) {
    case 1: newstate=GAMEK_BUTTON_UP; break;
    case 2: newstate=GAMEK_BUTTON_UP|GAMEK_BUTTON_RIGHT; break;
    case 3: newstate=GAMEK_BUTTON_RIGHT; break;
    case 4: newstate=GAMEK_BUTTON_RIGHT|GAMEK_BUTTON_DOWN; break;
    case 5: newstate=GAMEK_BUTTON_DOWN; break;
    case 6: newstate=GAMEK_BUTTON_DOWN|GAMEK_BUTTON_LEFT; break;
    case 7: newstate=GAMEK_BUTTON_LEFT; break;
    case 8: newstate=GAMEK_BUTTON_LEFT|GAMEK_BUTTON_UP; break;
  }
  // A quick check, if nothing changed get out.
  uint16_t oldstate=inmgr->state_by_playerid[playerid]&GAMEK_BUTTON_HAT;
  if (newstate==oldstate) return;
  // Then ignore oldstate and send each bit.
  gamek_inmgr_send_button(inmgr,playerid,GAMEK_BUTTON_LEFT,newstate&GAMEK_BUTTON_LEFT);
  gamek_inmgr_send_button(inmgr,playerid,GAMEK_BUTTON_RIGHT,newstate&GAMEK_BUTTON_RIGHT);
  gamek_inmgr_send_button(inmgr,playerid,GAMEK_BUTTON_UP,newstate&GAMEK_BUTTON_UP);
  gamek_inmgr_send_button(inmgr,playerid,GAMEK_BUTTON_DOWN,newstate&GAMEK_BUTTON_DOWN);
}

/* Fire an action. Unlike buttons and hats, there's no state to compare to. So hey easy.
 */

static void gamek_inmgr_send_action(struct gamek_inmgr *inmgr,uint16_t actionid) {
  if (inmgr->delegate.action) inmgr->delegate.action(actionid,inmgr->delegate.userdata);
}

/* Cleanup greeting.
 */
 
void gamek_inmgr_joystick_greeting_cleanup(struct gamek_inmgr_joystick_greeting *greeting) {
  if (greeting->capv) free(greeting->capv);
}

/* Add capabiliity to greeting.
 */

int gamek_inmgr_joystick_greeting_add_capability(
  struct gamek_inmgr_joystick_greeting *greeting,
  int btnid,int usage,int lo,int hi,int value
) {
  if (greeting->capc>=greeting->capa) {
    int na=greeting->capa+64;
    if (na>INT_MAX/sizeof(struct gamek_inmgr_joystick_capability)) return -1;
    void *nv=realloc(greeting->capv,sizeof(struct gamek_inmgr_joystick_capability)*na);
    if (!nv) return -1;
    greeting->capv=nv;
    greeting->capa=na;
  }
  struct gamek_inmgr_joystick_capability *cap=greeting->capv+greeting->capc++;
  cap->btnid=btnid;
  cap->usage=usage;
  cap->lo=lo;
  cap->hi=hi;
  cap->value=value;
  return 0;
}

/* Finish a greeting.
 */
 
static int gamek_inmgr_compare_cap(const void *a,const void *b) {
  const struct gamek_inmgr_joystick_capability *A=a,*B=b;
  if (A->btnid<B->btnid) return -1;
  if (A->btnid>B->btnid) return 1;
  return 0;
}
 
void gamek_inmgr_joystick_greeting_ready(struct gamek_inmgr_joystick_greeting *greeting) {
  if (!greeting) return;
  qsort(greeting->capv,greeting->capc,sizeof(struct gamek_inmgr_joystick_capability),gamek_inmgr_compare_cap);
}

/* Read capabilities from greeting.
 */
 
struct gamek_inmgr_joystick_capability *gamek_inmgr_joystick_greeting_get_capability(
  const struct gamek_inmgr_joystick_greeting *greeting,
  int btnid
) {
  int lo=0,hi=greeting->capc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct gamek_inmgr_joystick_capability *cap=greeting->capv+ck;
         if (btnid<cap->btnid) hi=ck;
    else if (btnid>cap->btnid) lo=ck+1;
    else return cap;
  }
  return 0;
}

struct gamek_inmgr_joystick_capability *gamek_inmgr_joystick_greeting_get_capability_by_usage(
  const struct gamek_inmgr_joystick_greeting *greeting,
  int usage
) {
  struct gamek_inmgr_joystick_capability *cap=greeting->capv;
  int i=greeting->capc;
  for (;i-->0;cap++) {
    if (cap->usage==usage) return cap;
  }
  return 0;
}

/* Connect joystick.
 */
 
int gamek_inmgr_connect_private(struct gamek_inmgr *inmgr,const struct gamek_inmgr_joystick_greeting *greeting) {
  struct gamek_inmgr_tm *tm=inmgr->tmv;
  int i=inmgr->tmc;
  for (;i-->0;tm++) {
    if (gamek_inmgr_tm_match(tm,greeting)) {
      return gamek_inmgr_map_device(inmgr,greeting,tm);
    }
  }
  if (tm=gamek_inmgr_synthesize_template(inmgr,greeting)) {
    fprintf(stderr,"Synthesized mapping for input device '%s'.\n",greeting->name);
    if (inmgr->delegate.config_dirty) inmgr->delegate.config_dirty(inmgr,inmgr->delegate.userdata);
    return gamek_inmgr_map_device(inmgr,greeting,tm);
  }
  fprintf(stderr,"No mapping for input device '%s', and we failed to make one up.\n",greeting->name);
  return 0;
}

int gamek_inmgr_joystick_connect(struct gamek_inmgr *inmgr,const struct gamek_inmgr_joystick_greeting *greeting) {
  if (!greeting->id) return -1; // id zero is reserved.
  return gamek_inmgr_connect_private(inmgr,greeting);
}

/* Disconnect joystick.
 */
 
int gamek_inmgr_joystick_disconnect(struct gamek_inmgr *inmgr,const void *id) {

  // Locate maps. If there aren't any, we're done.
  int mapp=gamek_inmgr_mapv_search(inmgr,id,0);
  if (mapp<0) mapp=-mapp-1;
  int mapc=0;
  while ((mapp+mapc<inmgr->mapc)&&(inmgr->mapv[mapp+mapc].id==id)) mapc++;
  if (!mapc) return 0;
  
  // Send OFF events for any held buttons. Record all playerid involved.
  uint8_t playeridhi=0;
  uint8_t dropped_playerid[256]={0};
  struct gamek_inmgr_map *map=inmgr->mapv+mapp;
  int i=mapc;
  for (;i-->0;map++) {
    if (!map->dstvalue) continue;
    if (!map->playerid) continue; // stateless
    dropped_playerid[map->playerid]=1;
    if (map->playerid>playeridhi) playeridhi=map->playerid;
    gamek_inmgr_send_button(inmgr,map->playerid,map->dstbtnid,0);
  }
  
  // Remove maps.
  inmgr->mapc-=mapc;
  memmove(inmgr->mapv+mapp,inmgr->mapv+mapp+mapc,sizeof(struct gamek_inmgr_map)*(inmgr->mapc-mapp));
  
  // Search remaining maps. If any previously-mapped is no longer reachable, send CD=0.
  for (map=inmgr->mapv,i=inmgr->mapc;i-->0;map++) {
    if (!map->playerid) continue;
    dropped_playerid[map->playerid]=0;
  }
  int playerid=1; for (;playerid<=playeridhi;playerid++) {
    if (dropped_playerid[playerid]) {
      gamek_inmgr_send_button(inmgr,playerid,GAMEK_BUTTON_CD,0);
    }
  }
  
  return 0;
}

/* Joystick event.
 */
 
int gamek_inmgr_joystick_event(struct gamek_inmgr *inmgr,const void *id,int srcbtnid,int value) {
  int mapp=gamek_inmgr_mapv_search(inmgr,id,srcbtnid);
  if (mapp<0) return 0;
  struct gamek_inmgr_map *map=inmgr->mapv+mapp;
  for (;(mapp<inmgr->mapc)&&(map->id==id)&&(map->srcbtnid==srcbtnid);mapp++,map++) {
  
    if (value==map->srcvalue) continue;
    map->srcvalue=value;
    
    if (!map->playerid) { // stateless actions
      if (value<map->srclo) continue;
      if (value>map->srchi) continue;
      gamek_inmgr_send_action(inmgr,map->dstbtnid);
      
    } else if (map->dstbtnid==GAMEK_BUTTON_HAT) { // rotational hats
      uint8_t dstvalue=0;
      if ((value>=map->srclo)&&(value<=map->srclo+7)) dstvalue=value-map->srclo+1;
      if (dstvalue==map->dstvalue) continue;
      map->dstvalue=dstvalue;
      gamek_inmgr_send_hat(inmgr,map->playerid,dstvalue);
      
    } else { // regular 2-state buttons
      int dstvalue=((value>=map->srclo)&&(value<=map->srchi))?1:0;
      if (dstvalue==map->dstvalue) continue;
      map->dstvalue=dstvalue;
      gamek_inmgr_send_button(inmgr,map->playerid,map->dstbtnid,dstvalue);
    }
  }
  return 1;
}

/* Keyboard event.
 */
 
int gamek_inmgr_keyboard_event(struct gamek_inmgr *inmgr,int keycode,int value) {
  // Keyboards map just like joysticks with the special id zero.
  // Luckily there is only one such "special" device; if that changes we'll need something firmer.
  return gamek_inmgr_joystick_event(inmgr,0,keycode,value);
}

/* Receive MIDI.
 */
 
int gamek_inmgr_midi_events(struct gamek_inmgr *inmgr,int devid,const void *_v,int c) {
  if (!inmgr->delegate.midi) return 0; // ...why did you call this function, owner?
  
  // Find the device.
  struct gamek_inmgr_midi *device=0;
  struct gamek_inmgr_midi *q=inmgr->midiv;
  int i=inmgr->midic;
  for (;i-->0;q++) {
    if (q->devid==devid) {
      device=q;
      break;
    }
  }
  
  // Handle farewells (empty input).
  if (c<1) {
    if (!device) return 0;
    inmgr->delegate.midi(device->chid,0xff,0,0,inmgr->delegate.userdata);
    if (device->chid!=0xff) {
      inmgr->delegate.midi(device->chid,0x00,0,0,inmgr->delegate.userdata);
    }
    int p=device-inmgr->midiv;
    inmgr->midic--;
    memmove(device,device+1,sizeof(struct gamek_inmgr_midi)*(inmgr->midic-p));
    return 0;
  }
  
  // If we don't have a device yet, create it and introduce it.
  // No need to look for the incoming introducer block. It's an empty SysEx, which we'll ignore anyway.
  if (!device) {
    if (inmgr->midic>=inmgr->midia) {
      int na=inmgr->midia+4;
      if (na>INT_MAX/sizeof(struct gamek_inmgr_midi)) return -1;
      void *nv=realloc(inmgr->midiv,sizeof(struct gamek_inmgr_midi)*na);
      if (!nv) return -1;
      inmgr->midiv=nv;
      inmgr->midia=na;
    }
    device=inmgr->midiv+inmgr->midic++;
    memset(device,0,sizeof(struct gamek_inmgr_midi));
    device->devid=devid;
    device->chid=0xff; //TODO Allow overriding MIDI channel.
    inmgr->delegate.midi(device->chid,0x01,0,0,inmgr->delegate.userdata);
  }
  
  const uint8_t *v=_v;
  for (;c-->0;v++) {
    
    // >=0xf8 is Realtime. Pass it right through.
    if (v[0]>=0xf8) {
      inmgr->delegate.midi(device->chid,v[0],0,0,inmgr->delegate.userdata);
      continue;
    }
    
    // 0xf0 and 0xf7 begin and end SysEx.
    if (v[0]==0xf0) {
      device->status=0xf0;
      continue;
    }
    if (v[0]==0xf7) {
      device->status=0x00;
      continue;
    }
    
    // If we're in Sysex mode, skip everything else.
    if (device->status==0xf0) {
      continue;
    }
    
    // 0xf1..0xf6 are undefined or oddballs, I don't expect to see them.
    if (v[0]>=0xf0) {
      device->status=0;
      continue;
    }
    
    // High bit set means it's a status byte.
    if (v[0]>=0x80) {
      device->status=v[0];
      device->a=0xff;
      device->b=0xff;
      continue;
    }
    
    // It's a data byte. Ignore if status unset, otherwise figure out which data byte it belongs to, and fire callback if complete.
    if (!device->status) {
      continue;
    }
    uint8_t chid=device->chid;
    if (chid==0xff) chid=device->status&0x0f;
    if (device->a==0xff) device->a=v[0];
    else device->b=v[0];
    switch (device->status&0xf0) {
      case 0x90: { // Note On. Check for velocity zero, otherwise it's a regular 2-byte.
          if (device->b==0xff) continue;
          if (!device->b) {
            inmgr->delegate.midi(chid,0x80,device->a,0x40,inmgr->delegate.userdata);
            device->a=0xff;
            device->b=0xff;
            continue;
          }
        } // pass
      case 0xa0:
      case 0xb0:
      case 0xe0:
      case 0x80: { // Regular 2-byte payloads.
          if (device->b!=0xff) {
            inmgr->delegate.midi(chid,device->status&0xf0,device->a,device->b,inmgr->delegate.userdata);
            device->a=0xff;
            device->b=0xff;
          }
        } break;
      case 0xc0:
      case 0xd0: { // Regular 1-byte payloads.
          inmgr->delegate.midi(chid,device->status&0xf0,device->a,0,inmgr->delegate.userdata);
          device->a=0xff;
          device->b=0xff;
        } break;
      default: { // oops? clear status
          device->status=0;
          device->a=0xff;
          device->b=0xff;
        }
    }
  }
  return 0;
}
