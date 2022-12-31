#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include <stdio.h>
#include <stdlib.h>

#define TILESIZE 16
#define HERO_LIMIT 8
#define WALK_FRAME_TIME 1
#define WALK_FRAME_COUNT 8
#define HERO_WALK_SPEED 1 /* hero positions are in pixels for simplicity. real life, you'd want a finer physics space */

extern const uint8_t font_g06[];
extern const uint8_t graphics[];

static struct gamek_image img_graphics={0};

static int16_t worldw=480,worldh=270; // will update at first render

/* herov is indexed by (playerid-1).
 * If (tileid_head) zero, it's unused.
 */
static struct hero {
  int16_t x,y; // center of legs tiles
  uint8_t tileid_head;
  uint8_t tileid_hair;
  uint8_t xform; // 0=right or GAMEK_XFORM_XREV=left
  uint8_t animframe; // 0..7, which legs to display
  uint8_t animclock;
  int8_t dx; // -1,0,1: Horizontal axis of dpad
} herov[HERO_LIMIT]={0};

/* Initialize app.
 * Return zero on success, or <0 if something fails.
 */

static int8_t hires_init() {
  fprintf(stderr,"%s\n",__func__);
  if (gamek_image_decode(&img_graphics,graphics,0x7fffffff)<0) return -1;
  return 0;
}

/* Initialize one hero.
 */
 
static void hero_init(struct hero *hero,uint8_t heroid) {
  // There are three heads and three hairs. Pick one randomly.
  hero->tileid_head=0x20+rand()%3;
  hero->tileid_hair=0x23+rand()%3;
  // Position at ground level (1.5 tiles from bottom), and horizontally according to (heroid).
  hero->x=(heroid*worldw)/HERO_LIMIT+(worldw/(HERO_LIMIT<<1));
  hero->y=worldh-TILESIZE-(TILESIZE>>1);
  // And the rest is constant.
  hero->xform=0; // face right
  hero->animframe=0;
  hero->animclock=0;
  hero->dx=0;
}

/* Update one hero.
 */
 
static void hero_update(struct hero *hero) {
  if (!hero->tileid_head) return; // not in use
  
  if (hero->dx) {
  
    if (hero->animclock) hero->animclock--; else {
      hero->animclock=WALK_FRAME_TIME;
      hero->animframe++;
      if (hero->animframe>=WALK_FRAME_COUNT) hero->animframe=0;
    }
    
    hero->x+=hero->dx*HERO_WALK_SPEED;
    if (hero->x<0) hero->x=0;
    else if (hero->x>worldw) hero->x=worldw;
    
  } else {
    hero->animframe=0;
    hero->animclock=0;
  }
}

/* Update.
 * This gets called at about 60 Hz.
 */

static void hires_update() {
  struct hero *hero=herov;
  uint8_t i=HERO_LIMIT;
  for (;i-->0;hero++) hero_update(hero);
}

/* Input.
 * Gamek does not track input state for you, you need to react to each event.
 * (playerid) is zero for the aggregate state of all player inputs.
 * For a one-player-only game, that's what you should use.
 */

static void hires_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
  fprintf(stderr,"%s %d.0x%04x=%d\n",__func__,playerid,btnid,value);
  if (playerid>0) {
    uint8_t heroid=playerid-1;
    if (heroid<HERO_LIMIT) {
      struct hero *hero=herov+heroid;
      
      // If carrier detect goes off, the device is disconnected and we should drop the hero cold.
      if ((btnid==GAMEK_BUTTON_CD)&&!value) {
        hero->tileid_head=0;
        return;
      }
      
      // Any other event, ensure the hero is initialized.
      // We could opt to do this at CD=1, which is supposed to be the first event for each playerid, but might as well do it on any event.
      if (!hero->tileid_head) {
        hero_init(hero,heroid);
      }
      
      if (value) switch (btnid) {
        case GAMEK_BUTTON_LEFT: hero->dx=-1; hero->xform=GAMEK_XFORM_XREV; break;
        case GAMEK_BUTTON_RIGHT: hero->dx=1; hero->xform=0; break;
        case GAMEK_BUTTON_SOUTH: break; // jump?
        case GAMEK_BUTTON_WEST: break; // fire? dash? run?
      } else switch (btnid) {
        case GAMEK_BUTTON_LEFT: if (hero->dx<0) { hero->dx=0; } break;
        case GAMEK_BUTTON_RIGHT: if (hero->dx>0) { hero->dx=0; } break;
      }
    }
  }
}

/* Render.
 * If you want to keep the previous frame, do nothing and return zero.
 * Otherwise, overwrite (fb) and return nonzero.
 */

static uint8_t hires_render(struct gamek_image *fb) {

  #if 0 //XXX Testing scale-up aliasing with a checkerboard.
  if (gamek_image_format_pixel_size_bits(fb->fmt)!=32) {
    // We're going to touch pixels directly, and we'll assume 32 bits per pixel.
    // That is correct for most platforms. Tiny, Thumby, and Pico will not work.
    return 0;
  }

  // Fill with a 1-pixel checkerboard pattern.
  uint32_t colorv[2]={
    gamek_image_pixel_from_rgba(fb->fmt,0xf0,0xc0,0x00,0xff),
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
  #endif
  
  //TODO I don't yet have a way to examine the framebuffer size before the first render.
  // Firgure something out. We ought to be setting (worldw) at init instead of here.
  worldw=fb->w;
  worldh=fb->h;
  
  // Background.
  uint32_t skycolor=gamek_image_pixel_from_rgba(fb->fmt,0x80,0xc0,0xff,0xff);
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,skycolor);
  
  // Terrain.
  int16_t dstx=0;
  int16_t dsty=fb->h-TILESIZE;
  for (;dstx<fb->w;dstx+=TILESIZE) {
    gamek_image_blit(fb,dstx,dsty,&img_graphics,TILESIZE,0,TILESIZE,TILESIZE,0);
  }
  
  // Sprites.
  struct hero *hero=herov;
  uint8_t i=HERO_LIMIT;
  for (;i-->0;hero++) {
    if (!hero->tileid_head) continue;
    gamek_image_blit_tile(fb,hero->x,hero->y,&img_graphics,TILESIZE,0x30+hero->animframe,hero->xform);
    gamek_image_blit_tile(fb,hero->x,hero->y-TILESIZE,&img_graphics,TILESIZE,hero->tileid_head,hero->xform);
    gamek_image_blit_tile(fb,hero->x,hero->y-TILESIZE,&img_graphics,TILESIZE,hero->tileid_hair,hero->xform);
  }
  
  //gamek_font_render_string(fb,10,10,gamek_image_pixel_from_rgba(fb->fmt,0xff,0xff,0xff,0xff),font_g06,"Hello cruel world!",-1);
  return 1;
}

/* Client definition.
 */
 
const struct gamek_client gamek_client={
  .title="Super Pixel Bros.",
  
  /* Here's the main thing this demo is for.
   * Most demos use a 96x64 framebuffer to remain compatible with Tiny.
   * We're doing software rendering, so you don't want to go crazy high, but one can certainly go above 96 pixels for PCs.
   * "hi-res" might not be the best name for it...
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

