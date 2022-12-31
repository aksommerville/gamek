#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include <stdio.h>

extern const uint8_t font_g06[];

/* Initialize app.
 * Return zero on success, or <0 if something fails.
 */

static int8_t hires_init() {
  return 0;
}

/* Update.
 * This gets called at about 60 Hz.
 */

static void hires_update() {
}

/* Input.
 * Gamek does not track input state for you, you need to react to each event.
 * (playerid) is zero for the aggregate state of all player inputs.
 * For a one-player-only game, that's what you should use.
 */

static void hires_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
  fprintf(stderr,"%s %d.0x%04x=%d\n",__func__,playerid,btnid,value);
}

/* Render.
 * If you want to keep the previous frame, do nothing and return zero.
 * Otherwise, overwrite (fb) and return nonzero.
 */

static uint8_t hires_render(struct gamek_image *fb) {

  if (gamek_image_format_pixel_size_bits(fb->fmt)!=32) {
    // We're going to touch pixels directly, and we'll assume 32 bits per pixel.
    // That is correct for most platforms. Tiny, Thumby, and Pico will not work.
    return 0;
  }

  // Fill with a 1-pixel checkerboard pattern.
  uint32_t colorv[2]={
    gamek_image_pixel_from_rgba(fb->fmt,0x80,0x60,0x00,0xff),
    gamek_image_pixel_from_rgba(fb->fmt,0x40,0x30,0x00,0xff),
  };
  uint8_t *dstrow=fb->v;
  int16_t yi=fb->h;
  for (;yi-->0;dstrow+=fb->stride) {
    uint32_t *dstp=(uint32_t*)dstrow;
    uint8_t colorp=(yi&1);
    int16_t xi=fb->w;
    for (;xi-->0;dstp++) {
      *dstp=colorv[colorp];
      colorp^=1;
    }
  }
  
  gamek_font_render_string(fb,10,10,gamek_image_pixel_from_rgba(fb->fmt,0xff,0xff,0xff,0xff),font_g06,"Hello cruel world!",-1);
  return 1;
}

/* Client definition.
 */
 
const struct gamek_client gamek_client={
  .title="Super Pixel Bros.",
  
  /* Here's the main thing this demo is for.
   * Most demos use a 96x64 framebuffer to remain compatible with Tiny.
   * We're doing software rendering, so you don't want to go crazy high, but one can certainly go above 96 pixels for PCs.
   * (480,270) is (1920,1080)/4, which happens to be the exact threshold where our X11 driver switches to nearest-neighbor scaling.
   * It will show crisp pixels fullscreen, and linear interpolation otherwise.
   */
  .fbw=480,
  .fbh=270,
  
  .iconrgba=0,
  .iconw=0,
  .iconh=0,
  .init=hires_init,
  .update=hires_update,
  .render=hires_render,
  .input_event=hires_input_event,
};

