#include "gamek_web_internal.h"

struct gamek_web gamek_web={0};
static uint8_t gamek_web_fb_storage[GAMEK_WEB_FB_LIMIT*GAMEK_WEB_FB_LIMIT*4];

/* gamek_client wrappers, for the JS app to call.
 */
 
int8_t gamek_web_client_init() {
  
  gamek_web_external_set_title(gamek_client.title);

  gamek_web.fb.w=gamek_client.fbw;
  gamek_web.fb.h=gamek_client.fbh;
  if (
    (gamek_web.fb.w<1)||(gamek_web.fb.w>GAMEK_WEB_FB_LIMIT)||
    (gamek_web.fb.h<1)||(gamek_web.fb.h>GAMEK_WEB_FB_LIMIT)
  ) {
    gamek_web.fb.w=128;
    gamek_web.fb.h=96;
  }
  gamek_web.fb.fmt=GAMEK_IMGFMT_RGBX;
  gamek_web.fb.stride=gamek_web.fb.w<<2;
  gamek_web.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;
  gamek_web.fb.v=gamek_web_fb_storage;
  gamek_web_external_set_framebuffer(gamek_web.fb.v,gamek_web.fb.w,gamek_web.fb.h);

  if (!gamek_client.init) return 0;
  return gamek_client.init();
}

void gamek_web_client_update() {
  if (gamek_client.update) gamek_client.update();
}

void *gamek_web_client_render() {
  if (!gamek_client.render) return 0;
  if (!gamek_client.render(&gamek_web.fb)) return 0;
  return gamek_web.fb.v;
}

void gamek_web_client_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
  if (!gamek_client.input_event) return;
  gamek_client.input_event(playerid,btnid,value);
}

void gamek_web_client_midi_in(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  if (!gamek_client.midi_in) return;
  gamek_client.midi_in(chid,opcode,a,b);
}

/* Platform definition.
 */
 
const struct gamek_platform_details gamek_platform_details={
  .name="web",
  .capabilities=
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
