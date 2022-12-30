#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>

#define SNAKE_COLC 12
#define SNAKE_ROWC 8
#define SNAKE_COLW 8
#define SNAKE_ROWH 8
#define SNAKE_GRID_X 0
#define SNAKE_GRID_Y 0

#define SNAKE_SNEGMENT_LIMIT (SNAKE_COLC*SNAKE_ROWC)
#define SNAKE_SNAK_LIMIT 8
#define SNAKE_STEP_TIME_INITIAL 40
#define SNAKE_STEP_TIME_MINIMUM 4

/* Globals.
 */
 
extern const uint8_t snakebits[];
extern const uint8_t font_g06[];
extern const uint8_t loungelizard[];
extern const uint8_t snakeshakin[];
extern const int16_t wave0[];
extern const int16_t wave1[];
extern const int16_t wave2[];

static struct gamek_image img_snakebits;

static struct snake {
  uint8_t x:4;
  uint8_t y:4;
  struct snegment {
    uint8_t dir; // GAMEK_BUTTON_(UP|DOWN|LEFT|RIGHT), relative to the prior snegment
  } snegmentv[SNAKE_SNEGMENT_LIMIT];
  uint8_t snegmentc; // Must be at least one. Head does not count as a snegment but tail does.
  uint8_t step_time;
  uint8_t alive;
} snake={0};

// There is always one snak. The moment it gets collected, we make a new one.
static struct snak {
  uint8_t x:4;
  uint8_t y:4;
} snak={0};

static uint8_t next_direction=0; // GAMEK_BUTTON_(UP|DOWN|LEFT|RIGHT)
static uint8_t step_time=0;
static uint8_t victory=0;
static uint8_t video_dirty=0;

/* Sound.
 */
 
static void sound_effect_start_game() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x60); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x01); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x00); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x50); // release time
  gamek_platform_audio_event(0x0f,0xb0,0x40,0x20); // attack level
  gamek_platform_audio_event(0x0f,0x90,0x30,0x40);
  gamek_platform_audio_event(0x0f,0x80,0x30,0x40);
}

static void sound_effect_game_over() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x20); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x08); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x20); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x50); // release time
  gamek_platform_audio_event(0x0f,0xb0,0x0d,0x40); // attack level
  gamek_platform_audio_event(0x0f,0x90,0x48,0x40);
  gamek_platform_audio_event(0x0f,0x80,0x48,0x40);
}

static void sound_effect_victory() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x20); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x02); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x00); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x48); // release time
  gamek_platform_audio_event(0x0f,0xb0,0x0d,0x40); // attack level
  gamek_platform_audio_event(0x0f,0x90,0x4c,0x40);
  gamek_platform_audio_event(0x0f,0x80,0x4c,0x40);
  gamek_platform_audio_event(0x0f,0x90,0x53,0x40);
  gamek_platform_audio_event(0x0f,0x80,0x53,0x40);
}

static void sound_effect_snak() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x40); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x05); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x00); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x0c); // release time
  gamek_platform_audio_event(0x0f,0xb0,0x0d,0x40); // attack level
  gamek_platform_audio_event(0x0f,0x90,0x40,0x40);
  gamek_platform_audio_event(0x0f,0x80,0x40,0x40);
}

static void sound_effect_tick() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x40); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x05); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x40); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x08); // release time
  gamek_platform_audio_event(0x0f,0xb0,0x0d,0x20); // attack level
  gamek_platform_audio_event(0x0f,0x90,0x38,0x40);
  gamek_platform_audio_event(0x0f,0x80,0x38,0x40);
}

/* Kill the snake.
 */
 
static void kill_snake(uint8_t win_everything) {
  snake.alive=0;
  victory=win_everything;
  video_dirty=1;
  gamek_platform_play_song(loungelizard,0x7fff);
  if (win_everything) sound_effect_victory();
  else sound_effect_game_over();
}

/* Pick a random unoccupied cell for the snak.
 */
 
static void new_snak() {

  // Fill an array of all possible cells with their own index.
  uint8_t candidatev[SNAKE_COLC*SNAKE_ROWC];
  uint8_t *p=candidatev;
  uint8_t i=SNAKE_COLC*SNAKE_ROWC,v=0;
  for (;i-->0;p++,v++) *p=v;
  
  // Trace the snake and set all his cells OOB.
  uint8_t x=snake.x,y=snake.y;
  candidatev[y*SNAKE_COLC+x]=0xff;
  const struct snegment *snegment=snake.snegmentv;
  for (i=snake.snegmentc;i-->0;snegment++) {
    switch (snegment->dir) {
      case GAMEK_BUTTON_LEFT: x--; break;
      case GAMEK_BUTTON_RIGHT: x++; break;
      case GAMEK_BUTTON_UP: y--; break;
      case GAMEK_BUTTON_DOWN: y++; break;
    }
    candidatev[y*SNAKE_COLC+x]=0xff;
  }
  
  // Eliminate OOB candidates.
  uint8_t candidatec=SNAKE_COLC*SNAKE_ROWC;
  uint8_t rmc=0;
  for (i=candidatec;i-->0;) {
    if (candidatev[i]==0xff) rmc++;
    else if (rmc) {
      candidatec-=rmc;
      memmove(candidatev+i+1,candidatev+i+1+rmc,candidatec-i-1);
      rmc=0;
    }
  }
  if (rmc) {
    candidatec-=rmc;
    memmove(candidatev,candidatev+rmc,candidatec);
  }
  
  // If there are no candidates, we must end the game.
  if (!candidatec) {
    kill_snake(1);
    return;
  }
  
  // Pick a cell at random.
  uint8_t choice=rand()%candidatec;
  snak.x=candidatev[choice]%SNAKE_COLC;
  snak.y=candidatev[choice]/SNAKE_COLC;
  video_dirty=1;
}

/* Reset game.
 */
 
static void snake_reset() {
  
  snake.snegmentc=1;
  snake.x=SNAKE_COLC>>1;
  snake.y=SNAKE_ROWC>>1;
  snake.snegmentv[0].dir=GAMEK_BUTTON_RIGHT;
  snake.step_time=SNAKE_STEP_TIME_INITIAL;
  snake.alive=1;
  
  next_direction=0;
  step_time=SNAKE_STEP_TIME_INITIAL;
  victory=0;
  
  new_snak();
}

/* Setup.
 */

static int8_t _snake_init() {

  if (gamek_image_decode(&img_snakebits,snakebits,0x7fffffff)<0) return -1;
  gamek_platform_set_audio_wave(0,wave0,512);
  gamek_platform_set_audio_wave(1,wave1,512);
  gamek_platform_set_audio_wave(2,wave2,512);
  
  snake_reset();
  gamek_platform_play_song(loungelizard,0x7fff);
  return 0;
}

/* Begin move to the next cell.
 */
 
static void move_head(int8_t dx,int8_t dy) {
  uint8_t neckdir;

  if (dx<0) {
    if (!snake.x) { kill_snake(0); return; }
    if (snake.snegmentv[0].dir==GAMEK_BUTTON_LEFT) return;
    snake.x--;
    neckdir=GAMEK_BUTTON_RIGHT;
    
  } else if (dx>0) {
    if (snake.x>=SNAKE_COLC-1) { kill_snake(0); return; }
    if (snake.snegmentv[0].dir==GAMEK_BUTTON_RIGHT) return;
    snake.x++;
    neckdir=GAMEK_BUTTON_LEFT;
    
  } else if (dy<0) {
    if (!snake.y) { kill_snake(0); return; }
    if (snake.snegmentv[0].dir==GAMEK_BUTTON_UP) return;
    snake.y--;
    neckdir=GAMEK_BUTTON_DOWN;
    
  } else if (dy>0) {
    if (snake.y>=SNAKE_ROWC-1) { kill_snake(0); return; }
    if (snake.snegmentv[0].dir==GAMEK_BUTTON_DOWN) return;
    snake.y++;
    neckdir=GAMEK_BUTTON_UP;
    
  } else {
    return;
  }
  
  // Cascade directions back to front.
  // There is hardly any snake state, so we can get away with just memmoving the list.
  uint8_t taildir=snake.snegmentv[snake.snegmentc-1].dir;
  memmove(snake.snegmentv+1,snake.snegmentv,sizeof(struct snegment)*(snake.snegmentc-1));
  snake.snegmentv[0].dir=neckdir;
  
  // Check self-collisions. In other words, walk the length of the snake excluding its head, and if any matches the head, it's a collision.
  // Body-on-body collision is not possible.
  uint8_t x=snake.x,y=snake.y;
  const struct snegment *snegment=snake.snegmentv;
  uint8_t i=snake.snegmentc;
  for (;i-->0;snegment++) {
    switch (snegment->dir) {
      case GAMEK_BUTTON_LEFT: x--; break;
      case GAMEK_BUTTON_RIGHT: x++; break;
      case GAMEK_BUTTON_UP: y--; break;
      case GAMEK_BUTTON_DOWN: y++; break;
    }
    if ((x==snake.x)&&(y==snake.y)) {
      kill_snake(0);
      return;
    }
  }
  
  // If we moved on to the snak, extend my tail and generate a new snak. (in that order, it matters).
  if ((snake.x==snak.x)&&(snake.y==snak.y)) {
    if (snake.snegmentc<SNAKE_SNEGMENT_LIMIT) {
      snake.snegmentv[snake.snegmentc++].dir=taildir;
    }
    new_snak();
    if (step_time>SNAKE_STEP_TIME_MINIMUM) step_time--;
    sound_effect_snak();
  } else {
    sound_effect_tick();
  }
  video_dirty=1;
}

/* Update.
 */

static void _snake_update() {
  if (snake.alive) {
    if (snake.step_time) {
      snake.step_time--;
    } else {
      snake.step_time=step_time;
      switch (next_direction) {
        case GAMEK_BUTTON_UP: move_head(0,-1); break;
        case GAMEK_BUTTON_DOWN: move_head(0,1); break;
        case GAMEK_BUTTON_LEFT: move_head(-1,0); break;
        case GAMEK_BUTTON_RIGHT: move_head(1,0); break;
        // It's initially zero, and the snake stands still, by design.
      }
    }
  }
}

/* Input.
 */

static void _snake_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
  if ((playerid==0)&&value) {
    if (snake.alive) switch (btnid) {
      case GAMEK_BUTTON_UP:
      case GAMEK_BUTTON_DOWN:
      case GAMEK_BUTTON_LEFT:
      case GAMEK_BUTTON_RIGHT: {
          if (snake.snegmentv[0].dir==btnid) return; // not allowed to turn 180
          if (!next_direction) {
            gamek_platform_play_song(snakeshakin,0x7fff);
          }
          next_direction=btnid;
        } break;
    } else switch (btnid) {
      case GAMEK_BUTTON_SOUTH:
      case GAMEK_BUTTON_WEST:
      case GAMEK_BUTTON_EAST:
      case GAMEK_BUTTON_NORTH: {
          sound_effect_start_game();
          snake_reset();
        } break;
    }
  }
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
 
static void fake_printf(char *dst,uint16_t dsta,const char *fmt,...) {
  if (dsta<1) return;
  dst[0]=0;
  if (!fmt) return;
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
      return;
    }
  }
  dst[dstc]=0;
}

/* Render.
 */

static uint8_t _snake_render(struct gamek_image *fb) {
  if (!video_dirty) return 0;
  video_dirty=0;

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
  
  // Snak.
  gamek_image_blit(
    fb,SNAKE_GRID_X+snak.x*SNAKE_COLW,SNAKE_GRID_Y+snak.y*SNAKE_ROWH,
    &img_snakebits,SNAKE_COLW,0,
    SNAKE_COLW,SNAKE_ROWH,0
  );
  
  // Snake.
  int16_t srcy=(victory?3:snake.alive?1:2)*SNAKE_ROWH;
  int16_t dstx=SNAKE_GRID_X+snake.x*SNAKE_COLW;
  dsty=SNAKE_GRID_Y+snake.y*SNAKE_ROWH;
  const struct snegment *snegment=0,*next=snake.snegmentv;
  uint8_t i=snake.snegmentc+1;
  for (;i-->0;snegment=next,next++) {
    if (snegment) {
      if (i) { // body. There's 12 possible arrangements. I'll just enumerate them instead of trying to get clever about it.
        int16_t srcx;
        uint8_t xform;
        switch ((snegment->dir<<4)|next->dir) {
          case (GAMEK_BUTTON_LEFT<<4)|GAMEK_BUTTON_LEFT: srcx=SNAKE_COLW; xform=GAMEK_XFORM_XREV; break;
          case (GAMEK_BUTTON_LEFT<<4)|GAMEK_BUTTON_UP: srcx=SNAKE_COLW*3; xform=GAMEK_XFORM_XREV|GAMEK_XFORM_YREV; break;
          case (GAMEK_BUTTON_LEFT<<4)|GAMEK_BUTTON_DOWN: srcx=SNAKE_COLW*3; xform=GAMEK_XFORM_XREV; break;
          case (GAMEK_BUTTON_RIGHT<<4)|GAMEK_BUTTON_RIGHT: srcx=SNAKE_COLW; xform=0; break;
          case (GAMEK_BUTTON_RIGHT<<4)|GAMEK_BUTTON_UP: srcx=SNAKE_COLW*4; xform=GAMEK_XFORM_YREV; break;
          case (GAMEK_BUTTON_RIGHT<<4)|GAMEK_BUTTON_DOWN: srcx=SNAKE_COLW*3; xform=0; break;
          case (GAMEK_BUTTON_UP<<4)|GAMEK_BUTTON_LEFT: srcx=SNAKE_COLW*3; xform=GAMEK_XFORM_XREV|GAMEK_XFORM_YREV|GAMEK_XFORM_SWAP; break;
          case (GAMEK_BUTTON_UP<<4)|GAMEK_BUTTON_RIGHT: srcx=SNAKE_COLW*3; xform=GAMEK_XFORM_SWAP|GAMEK_XFORM_XREV; break;
          case (GAMEK_BUTTON_UP<<4)|GAMEK_BUTTON_UP: srcx=SNAKE_COLW; xform=GAMEK_XFORM_SWAP|GAMEK_XFORM_XREV; break;
          case (GAMEK_BUTTON_DOWN<<4)|GAMEK_BUTTON_LEFT: srcx=SNAKE_COLW*3; xform=GAMEK_XFORM_SWAP|GAMEK_XFORM_YREV; break;
          case (GAMEK_BUTTON_DOWN<<4)|GAMEK_BUTTON_RIGHT: srcx=SNAKE_COLW*4; xform=GAMEK_XFORM_XREV|GAMEK_XFORM_YREV; break;
          case (GAMEK_BUTTON_DOWN<<4)|GAMEK_BUTTON_DOWN: srcx=SNAKE_COLW; xform=GAMEK_XFORM_SWAP|GAMEK_XFORM_YREV; break;
        }
        gamek_image_blit(fb,dstx,dsty,&img_snakebits,srcx,srcy,SNAKE_COLW,SNAKE_ROWH,xform);
      } else { // tail
        uint8_t xform;
        switch (snegment->dir) {
          case GAMEK_BUTTON_LEFT: xform=GAMEK_XFORM_XREV; break;
          case GAMEK_BUTTON_RIGHT: xform=0; break;
          case GAMEK_BUTTON_UP: xform=GAMEK_XFORM_XREV|GAMEK_XFORM_SWAP; break;
          case GAMEK_BUTTON_DOWN: xform=GAMEK_XFORM_YREV|GAMEK_XFORM_SWAP; break;
        }
        gamek_image_blit(fb,dstx,dsty,&img_snakebits,2*SNAKE_COLW,srcy,SNAKE_COLW,SNAKE_ROWH,xform);
      }
    } else { // head
      uint8_t xform;
      switch (next->dir) {
        case GAMEK_BUTTON_LEFT: xform=GAMEK_XFORM_XREV; break;
        case GAMEK_BUTTON_RIGHT: xform=0; break;
        case GAMEK_BUTTON_UP: xform=GAMEK_XFORM_XREV|GAMEK_XFORM_SWAP; break;
        case GAMEK_BUTTON_DOWN: xform=GAMEK_XFORM_YREV|GAMEK_XFORM_SWAP; break;
      }
      gamek_image_blit(fb,dstx,dsty,&img_snakebits,0,srcy,SNAKE_COLW,SNAKE_ROWH,xform);
    }
    if (next) switch (next->dir) {
      case GAMEK_BUTTON_LEFT: dstx-=SNAKE_COLW; break;
      case GAMEK_BUTTON_RIGHT: dstx+=SNAKE_COLW; break;
      case GAMEK_BUTTON_UP: dsty-=SNAKE_ROWH; break;
      case GAMEK_BUTTON_DOWN: dsty+=SNAKE_ROWH; break;
    }
  }
  
  // Game Over message.
  if (victory) {
    uint32_t bgcolor=gamek_image_pixel_from_rgba(fb->fmt,0x00,0x00,0x00,0xff);
    uint32_t fgcolor=gamek_image_pixel_from_rgba(fb->fmt,0xff,0xff,0x00,0xff);
    gamek_image_fill_rect(fb,0,29,fb->w,8,bgcolor);
    gamek_font_render_string(fb,30,29,fgcolor,font_g06,"Perfect!!!",10);
  } else if (!snake.alive) {
    uint32_t bgcolor=gamek_image_pixel_from_rgba(fb->fmt,0x00,0x00,0x00,0xff);
    uint32_t fgcolor=gamek_image_pixel_from_rgba(fb->fmt,0xff,0xff,0xff,0xff);
    char message[16];
    fake_printf(message,sizeof(message),"Score: %/%",snake.snegmentc,SNAKE_COLC*SNAKE_ROWC);
    gamek_image_fill_rect(fb,0,29,fb->w,8,bgcolor);
    gamek_font_render_string(fb,20,29,fgcolor,font_g06,message,-1);
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
