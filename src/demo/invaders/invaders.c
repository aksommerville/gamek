#include "pf/gamek_pf.h"
#include "common/gamek_image.h"

//TODO

static uint8_t luma=0;

static int8_t _invaders_init() {
  return 0;
}

static void _invaders_update() {
}

static uint8_t _invaders_render(struct gamek_image *fb) {
  luma++;
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,gamek_image_pixel_from_rgba(fb->fmt,luma,luma,luma,0xff));
  return 1;
}

static void _invaders_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
}

/* Client definition.
 */

const struct gamek_client gamek_client={
  .title="invaders",
  .fbw=96,
  .fbh=64,
  .init=_invaders_init,
  .update=_invaders_update,
  .render=_invaders_render,
  .input_event=_invaders_input_event,
};
