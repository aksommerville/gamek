#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include "common/gamek_map.h"
#include "dialogue.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>

#define TILESIZE 16
#define HERO_LIMIT 8
#define WALK_FRAME_TIME 1
#define WALK_FRAME_COUNT 8
#define HERO_WALK_SPEED 1 /* hero positions are in pixels for simplicity. real life, you'd want a finer physics space */

extern const uint8_t font_g06[];
extern const uint8_t graphics[];
extern const uint8_t home[];

static struct gamek_image img_graphics={0};
static struct gamek_map map={0};

static int16_t worldw=480,worldh=256; // Will update at init. Framebuffer size.

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
  struct gamek_image *dialogue;
  int16_t dialogue_dx; // left edge of dialogue box relative to hero's horizontal center
} herov[HERO_LIMIT]={0};

/* Initialize app.
 * Return zero on success, or <0 if something fails.
 */

static int8_t hires_init() {

  if (gamek_image_decode(&img_graphics,graphics,0x7fffffff)<0) return -1;
  if (gamek_map_decode(&map,home)<0) return -1;
  
  const struct gamek_image *fb=gamek_platform_get_sample_framebuffer();
  if (!fb) return -1;
  worldw=fb->w;
  worldh=fb->h;
  
  return 0;
}

/* Fake printf. When building for web, and I think tiny too, there is no libc snprintf.
 * Like snprintf, we always NUL-terminate.
 */
 
static uint16_t int_repr(char *dst,uint16_t dsta,int v) {
  if (v<0) {
    uint16_t dstc=2;
    int limit=-10;
    while (v<=limit) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    uint16_t i=dstc;
    for (;i-->1;v/=10) dst[i]='0'-v%10;
    dst[0]='-';
    return dstc;
  } else {
    uint16_t dstc=1;
    int limit=10;
    while (v>=limit) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    uint16_t i=dstc;
    for (;i-->0;v/=10) dst[i]='0'+v%10;
    return dstc;
  }
}
 
static int fake_printf(char *dst,uint16_t dsta,const char *fmt,...) {
  if (dsta<1) return 0;
  dst[0]=0;
  if (!fmt) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  uint16_t dstc=0;
  for (;*fmt;fmt++) {
    if (*fmt=='%') {
      int v=va_arg(vargs,int);
      dstc+=int_repr(dst+dstc,dsta-dstc,v);
    } else {
      dst[dstc++]=*fmt;
    }
    if (dstc>=dsta) {
      dst[dsta-1]=0;
      return dsta-1;
    }
  }
  dst[dstc]=0;
  return dstc;
}

/* Count initialized heroes.
 */
 
static uint8_t count_heroes() {
  uint8_t heroc=0;
  uint8_t i=HERO_LIMIT;
  const struct hero *hero=herov;
  for (;i-->0;hero++) {
    if (hero->tileid_head) heroc++;
  }
  return heroc;
}

/* Initialize one hero.
 */
 
static void hero_init(struct hero *hero,uint8_t heroid) {

  // There are three heads and three hairs. Pick one randomly, but the first three must be unique.
  uint8_t headv[3]={0x20,0x21,0x22};
  uint8_t headc=3;
  uint8_t hairv[3]={0x23,0x24,0x25};
  uint8_t hairc=3;
  const struct hero *other=herov;
  uint8_t i=HERO_LIMIT,j;
  for (;i-->0;other++) {
    if (!other->tileid_head) continue;
    for (j=headc;j-->0;) if (headv[j]==other->tileid_head) {
      headc--;
      memmove(headv+j,headv+j+1,headc-j);
    }
    for (j=hairc;j-->0;) if (hairv[j]==other->tileid_hair) {
      hairc--;
      memmove(hairv+j,hairv+j+1,hairc-j);
    }
  }
  if (!headc) hero->tileid_head=0x20+rand()%3;
  else hero->tileid_head=headv[rand()%headc];
  if (!hairc) hero->tileid_hair=0x23+rand()%3;
  else hero->tileid_hair=hairv[rand()%hairc];

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

/* Hero dialogue bubbles.
 * Or should it be "monologue"?
 */
 
static void hero_begin_dialogue(struct hero *hero) {
  if (hero->dialogue) return;
  int heroc=count_heroes();
  char msg[128];
  int msgc;
  if (!(gamek_platform_details.capabilities&GAMEK_PLATFORM_CAPABILITY_MULTIPLAYER)) {
    msgc=fake_printf(msg,sizeof(msg),"This build only supports one player. That's me!");
  } else if (heroc<=1) {
    msgc=fake_printf(msg,sizeof(msg),"I'm lonely. Connect more joysticks!");
  } else if (heroc<HERO_LIMIT) {
    // A better-made game would deal with the plural here.
    msgc=fake_printf(msg,sizeof(msg),"% friends is nice, but I'd rather have %. Connect more joysticks!",heroc-1,HERO_LIMIT-1);
  } else {
    msgc=fake_printf(msg,sizeof(msg),"Wow, where did you find % joysticks?",heroc);
  }
  hero->dialogue=dialogue_new(&hero->dialogue_dx,hero->x,worldw,&img_graphics,msg,msgc);
}

static void hero_end_dialogue(struct hero *hero) {
  if (!hero->dialogue) return;
  dialogue_del(hero->dialogue);
  hero->dialogue=0;
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
        case GAMEK_BUTTON_WEST: hero_begin_dialogue(hero); break;
      } else switch (btnid) {
        case GAMEK_BUTTON_LEFT: if (hero->dx<0) { hero->dx=0; } break;
        case GAMEK_BUTTON_RIGHT: if (hero->dx>0) { hero->dx=0; } break;
        case GAMEK_BUTTON_WEST: hero_end_dialogue(hero); break;
      }
    }
  }
}

/* Render.
 * If you want to keep the previous frame, do nothing and return zero.
 * Otherwise, overwrite (fb) and return nonzero.
 */

static uint8_t hires_render(struct gamek_image *fb) {
  
  // Background.
  uint32_t skycolor=gamek_image_pixel_from_rgba(fb->fmt,0x80,0xc0,0xff,0xff);
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,skycolor);
  
  // Terrain.
  // I've arranged for maps to be exactly the size of the framebuffer.
  // Platforms can give us a different framebuffer size, and if that happens, this will get weird.
  const int16_t map_worldh=map.h*TILESIZE;
  int16_t dsty=fb->h-map_worldh+(TILESIZE>>1);
  const uint8_t *mapp=map.v;
  uint8_t yi=map.h;
  for (;yi-->0;dsty+=TILESIZE) {
    int16_t dstx=TILESIZE>>1;
    uint8_t xi=map.w;
    for (;xi-->0;dstx+=TILESIZE,mapp++) {
      if (!*mapp) continue; // tile zero is transparent and we skip it
      gamek_image_blit_tile(fb,dstx,dsty,&img_graphics,TILESIZE,*mapp,0);
    }
  }
  
  // Sprites.
  struct hero *hero=herov;
  uint8_t i=HERO_LIMIT;
  for (;i-->0;hero++) {
    if (!hero->tileid_head) continue;
    gamek_image_blit_tile(fb,hero->x,hero->y,&img_graphics,TILESIZE,0x30+hero->animframe,hero->xform);
    gamek_image_blit_tile(fb,hero->x,hero->y-TILESIZE,&img_graphics,TILESIZE,hero->tileid_head,hero->xform);
    gamek_image_blit_tile(fb,hero->x,hero->y-TILESIZE,&img_graphics,TILESIZE,hero->tileid_hair,hero->xform);
    if (hero->dialogue) {
      gamek_image_blit(
        fb,hero->x+hero->dialogue_dx,hero->y-(TILESIZE*13)/8-hero->dialogue->h,
        hero->dialogue,0,0,
        hero->dialogue->w,hero->dialogue->h,
        0
      );
    }
  }
  
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
  .fbh=256,
  
  .iconrgba=0,
  .iconw=0,
  .iconh=0,
  .init=hires_init,
  .update=hires_update,
  .render=hires_render,
  .input_event=hires_input_event,
};

