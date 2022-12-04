#include "gamek_tiny_internal.h"

struct gamek_tiny gamek_tiny={0};

/* Setup.
 */
 
void setup() {

  tiny_video_init();
  tiny_audio_init();
  
  gamek_tiny.fb.v=gamek_tiny.fb_storage;
  gamek_tiny.fb.w=GAMEK_TINY_FBW;
  gamek_tiny.fb.h=GAMEK_TINY_FBH;
  gamek_tiny.fb.fmt=GAMEK_IMGFMT_BGR332;
  gamek_tiny.fb.stride=gamek_tiny.fb.w;
  gamek_tiny.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;

  if (gamek_client.init) {
    if (gamek_client.init()<0) {
      gamek_tiny.terminate=1;
      return;
    }
  }
  
  if (gamek_client.input_event) {
    gamek_client.input_event(1,GAMEK_BUTTON_CD,1);
    gamek_client.input_event(0,GAMEK_BUTTON_CD,1);
  }
}

/* Poll input.
 */
 
static void tiny_poll_input() {
  uint16_t state=GAMEK_BUTTON_CD;
  if (analogRead(42)<0x08) state|=GAMEK_BUTTON_UP;
  if (analogRead(19)<0x08) state|=GAMEK_BUTTON_DOWN;
  if (analogRead(25)<0x08) state|=GAMEK_BUTTON_LEFT;
  if (analogRead(15)<0x08) state|=GAMEK_BUTTON_RIGHT;
  if (!digitalRead(44)) state|=GAMEK_BUTTON_SOUTH;
  if (!digitalRead(45)) state|=GAMEK_BUTTON_WEST;
  if (state!=gamek_tiny.input) {
    uint16_t bit=1;
    for (;bit;bit<<=1) {
      if ((state&bit)&&!(gamek_tiny.input&bit)) {
        gamek_client.input_event(1,bit,1);
        gamek_client.input_event(0,bit,1);
      } else if (!(state&bit)&&(gamek_tiny.input&bit)) {
        gamek_client.input_event(1,bit,0);
        gamek_client.input_event(0,bit,0);
      }
    }
    gamek_tiny.input=state;
  }
}

/* Update.
 */

void loop() {

  // We're bare-metal and can't actually terminate (or can we?).
  // If termination was requested, sleep forever.
  if (gamek_tiny.terminate) {
    delay(1000);
    return;
  }
  
  if (gamek_client.input_event) {
    tiny_poll_input();
  }
  
  if (gamek_client.update) {
    gamek_client.update();
  }
  
  if (gamek_client.render) {
    if (gamek_client.render(&gamek_tiny.fb)) {
      tiny_video_swap(gamek_tiny.fb.v);
    }
  }
}

/* Terminate, public.
 */
 
void gamek_platform_terminate(uint8_t status) {
  gamek_tiny.terminate=1;
}

/* Platform definition.
 */
 
const struct gamek_platform_details gamek_platform_details={
  .name="tiny",
  .capabilities=
    GAMEK_PLATFORM_CAPABILITY_AUDIO|
    GAMEK_PLATFORM_CAPABILITY_STORAGE|
  0,
};
