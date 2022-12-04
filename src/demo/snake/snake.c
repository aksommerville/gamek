#include "pf/gamek_pf.h"
#include "common/gamek_image.h"

//TODO

static uint8_t color=0;

static int8_t _snake_init() {
  return 0;
}

static void _snake_update() {
}

static uint8_t _snake_render(struct gamek_image *fb) {
  color++;
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,gamek_image_pixel_from_rgba(fb->fmt,0,0,color,0xff));
  return 1;
}

static void _snake_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
}

/* Client definition.
 */

const struct gamek_client gamek_client={
  .title="snake",
  .fbw=96,
  .fbh=64,
  .init=_snake_init,
  .update=_snake_update,
  .render=_snake_render,
  .input_event=_snake_input_event,
};
