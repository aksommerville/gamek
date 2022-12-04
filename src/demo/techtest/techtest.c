#include "pf/gamek_pf.h"
#include "common/gamek_image.h"

//TODO

static uint8_t color=0;

static int8_t _techtest_init() {
  return 0;
}

static void _techtest_update() {
}

static uint8_t _techtest_render(struct gamek_image *fb) {
  color++;
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,gamek_image_pixel_from_rgba(fb->fmt,color,0,0,0xff));
  return 1;
}

static void _techtest_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
}

/* Client definition.
 */

const struct gamek_client gamek_client={
  .title="techtest",
  .fbw=96,
  .fbh=64,
  .init=_techtest_init,
  .update=_techtest_update,
  .render=_techtest_render,
  .input_event=_techtest_input_event,
};
