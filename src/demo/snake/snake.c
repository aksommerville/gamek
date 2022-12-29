#include "pf/gamek_pf.h"
#include "common/gamek_image.h"

#define SNAKE_COLC 10
#define SNAKE_ROWC 7
#define SNAKE_COLW 8
#define SNAKE_ROWH 8
#define SNAKE_GRID_X 8
#define SNAKE_GRID_Y 4

/* Globals.
 */
 
extern const uint8_t snakebits[];

static struct gamek_image img_snakebits;

/* Setup.
 */

static int8_t _snake_init() {
  if (gamek_image_decode(&img_snakebits,snakebits,0x7fffffff)<0) return -1;
  return 0;
}

/* Update.
 */

static void _snake_update() {
}

/* Input.
 */

static void _snake_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
}

/* Render.
 */

static uint8_t _snake_render(struct gamek_image *fb) {

  // Background.
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,0);
  int16_t dsty=SNAKE_GRID_Y;
  uint8_t yi=SNAKE_ROWC;
  for (;yi-->0;dsty+=SNAKE_ROWH) {
    int16_t dstx=SNAKE_GRID_X;
    uint8_t xi=SNAKE_COLC;
    for (;xi-->0;dstx+=SNAKE_COLW) {
      gamek_image_blit(fb,dstx,dsty,&img_snakebits,0,0,SNAKE_COLW,SNAKE_ROWH,0);
    }
  }
  
  return 1;
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
