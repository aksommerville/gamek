#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MM_PER_PIXEL_BITS 6
#define MM_PER_PIXEL (1<<MM_PER_PIXEL_BITS)

#define FBW_PIXELS 96
#define FBH_PIXELS 64
#define FBW_MM (FBW_PIXELS*MM_PER_PIXEL)
#define FBH_MM (FBH_PIXELS*MM_PER_PIXEL)

#define SHIP_SPEED_MAX_MM 128 /* "ship" is the hero sprite */
#define SHIP_ACCELERATION 10 /* mm/frame^2 */
#define SHIP_DECELERATION 20 /* mm/frame^2 */
#define SHIP_LEFT_BOUND (4*MM_PER_PIXEL)
#define SHIP_RIGHT_BOUND (FBW_MM-(5*MM_PER_PIXEL))

#define LASER_LIMIT 16 /* "laser" are the missiles fired by our ship */
#define LASER_H_TARGET 300
#define LASER_SPEED 256

#define MONSTER_LIMIT 32 /* "monster" are the enemy ships */
#define MONSTER_SPEED 32
#define MONSTER_LEFT_BOUND (4*MM_PER_PIXEL)
#define MONSTER_RIGHT_BOUND (FBW_MM-(5*MM_PER_PIXEL))

#define STAR_LIMIT 32 /* "star" are a purely decorative background element */
#define STAR_SPEED_MAX 128 /* brighter stars move faster, to enhance the parallax effect */
#define STAR_SPEED_MIN 16

#define ROCK_LIMIT 16 /* "rock" are the missiles fired by monsters at the hero */
#define ROCK_X_MIN (MM_PER_PIXEL*-4)
#define ROCK_Y_MIN (MM_PER_PIXEL*-4)
#define ROCK_X_MAX (FBW_MM+MM_PER_PIXEL*4)
#define ROCK_Y_MAX (FBH_MM+MM_PER_PIXEL*4)

#define GAME_OVER_DELAY 60
#define END_WAVE_DELAY 180

#define GAME_STATE_PLAY 1
#define GAME_STATE_END_WAVE 2
#define GAME_STATE_GAME_OVER 3

#define POINTS_KILL_INITIAL 1 /* kills are worth more if there's no intervening miss */
#define POINTS_KILL_INCREMENT 1
#define POINTS_KILL_MAX 10

#define FIREWORK_LIMIT 16
#define FIREWORK_TTL 60

extern const uint8_t font_g06[];
extern const uint8_t font_digits_3x5[];
extern const uint8_t aliensprites[];
extern const uint8_t defend_the_galaxy[];
extern const uint8_t supernova[];
extern const int16_t bass[];
extern const int16_t lead[];

static struct gamek_image img_aliensprites;
static uint8_t animclock=0; // everything that animates uses 4 frames and a shared clock
static uint8_t animframe=0;
static int16_t shipx=FBW_MM>>1; // middle of ship, in mm
static int8_t shipdx=0; // input state: -1,0,1
static int8_t shipdxsticky=0; // Tracks (shipdx) but does not return to zero.
static int16_t shipspeed=0; // slides up to SHIP_SPEED_MAX_MM while moving
static uint8_t alive=0;
static uint8_t explodeclock=0;
static uint8_t explodeframe=0;
static uint8_t game_over_delay=0;
static uint8_t current_wave=0;
static uint32_t score=0;
static uint32_t hiscore=0;
static uint32_t hiscore_color=0;
static uint8_t game_state=GAME_STATE_GAME_OVER;
static int16_t messagex=0,messagey=0;
static char message[32]="Defend the galaxy!";
static uint8_t messagec=18;
static uint8_t points_kill=POINTS_KILL_INITIAL;

static struct laser {
  int16_t x,y,h; // bounds in mm, width is always zero. h==0 if unused
} laserv[LASER_LIMIT]={0};

static struct monster {
  uint8_t tileid; // First tile, they all have 4 adjacent frames. Zero if unused.
  int16_t x,y; // Center
  int16_t radius;
} monsterv[MONSTER_LIMIT]={0};
static int16_t monsterdx=0;
static int16_t monsterdy=0;
static int16_t monsterdyc=0;

static struct star {
  int16_t x,y;
  uint32_t pixel;
  int16_t dy;
} starv[STAR_LIMIT]={0};

static struct rock {
  int16_t x,y;
  int16_t dx,dy;
  uint8_t tileid;
} rockv[ROCK_LIMIT]={0};
static uint16_t rocktime=0;
static uint16_t rockspeed=0;
static uint16_t rocktimelo=0;
static uint16_t rocktimehi=0;

static struct firework {
  int16_t x,y; // pixels, top left of text
  char msg[5];
  uint8_t msgc;
  uint8_t ttl;
} fireworkv[FIREWORK_LIMIT]={0};

/* Sound.
 */
 
static void sound_effect_start_game() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x60); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x01); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x00); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x50); // release time
  gamek_platform_audio_event(0x0f,0x90,0x30,0x40);
  gamek_platform_audio_event(0x0f,0x80,0x30,0x40);
}
 
static void sound_effect_end_wave() {
}
 
static void sound_effect_throw_rock() {
}
 
static void sound_effect_laser() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x70); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x10); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x40); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x10); // release time
  gamek_platform_audio_event(0x0f,0x90,0x4d,0x7f);
  gamek_platform_audio_event(0x0f,0x80,0x4d,0x40);
}
 
static void sound_effect_hero_dead() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x60); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x01); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x40); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x50); // release time
  gamek_platform_audio_event(0x0f,0x90,0x30,0x7f);
  gamek_platform_audio_event(0x0f,0x80,0x30,0x40);
}
 
static void sound_effect_monster_dead(uint8_t score) {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x20); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x20); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x20); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x10); // release time
  gamek_platform_audio_event(0x0f,0x90,0x30,0x7f);
  gamek_platform_audio_event(0x0f,0x80,0x30,0x40);
}
 
static void sound_effect_combo_lost() {
  gamek_platform_audio_event(0x0f,0xb0,0x48,0x70); // warble range
  gamek_platform_audio_event(0x0f,0xb0,0x49,0x01); // warble rate
  gamek_platform_audio_event(0x0f,0xb0,0x4a,0x40); // warble phase
  gamek_platform_audio_event(0x0f,0xb0,0x10,0x10); // release time
  gamek_platform_audio_event(0x0f,0x90,0x30,0x7f);
  gamek_platform_audio_event(0x0f,0x80,0x30,0x40);
}

static void sound_effects_setup() {
  gamek_platform_set_audio_wave(0,lead,512);
  gamek_platform_set_audio_wave(1,bass,512);
}

/* High score.
 */
 
static void load_hiscore() {
  uint8_t tmp[4]={0};
  gamek_platform_file_read(tmp,sizeof(tmp),"/HISCORE.U32",0);
  hiscore=(tmp[0]<<24)|(tmp[1]<<16)|(tmp[2]<<8)|tmp[3];
}

static void save_hiscore() {
  uint8_t tmp[4]={
    hiscore>>24,
    hiscore>>16,
    hiscore>>8,
    hiscore,
  };
  gamek_platform_file_write_all("/HISCORE.U32",tmp,sizeof(tmp));
}

/* Drop all lasers and rocks.
 */
 
static void drop_missiles() {
  memset(laserv,0,sizeof(laserv));
  memset(rockv,0,sizeof(rockv));
}

/* Set counter to the next rock.
 */
 
static void randomize_rock_time() {
  rocktime=rocktimelo+rand()%(rocktimehi-rocktimelo+1);
}

/* Make a new rock randomly.
 */
 
static void rock_generate() {
  struct rock *rock=0;
  struct rock *q=rockv;
  uint8_t i=ROCK_LIMIT;
  for (;i-->0;q++) {
    if (!q->tileid) {
      rock=q;
      break;
    }
  }
  if (!rock) return;
  
  // Pick a monster from the top row. Pick randomly if it's a tie, which is likely.
  uint8_t monsterc=0;
  int16_t monstery=0x7fff;
  struct monster *monster=monsterv;
  for (i=MONSTER_LIMIT;i-->0;monster++) {
    if (!monster->tileid) continue;
    if (monster->y<monstery) {
      monsterc=0;
      monstery=monster->y;
    }
    if (monster->y==monstery) {
      monsterc++;
    }
  }
  if (!monsterc) return; // oops, are they all dead?
  uint8_t monsterp=rand()%monsterc;
  for (monster=monsterv,i=MONSTER_LIMIT;i-->0;monster++) {
    if (!monster->tileid) continue;
    if (monster->y!=monstery) continue;
    if (!monsterp--) break;
  }
  
  sound_effect_throw_rock();
  rock->x=monster->x;
  rock->y=monster->y;
  rock->tileid=0x20;
  rock->dy=59*MM_PER_PIXEL-monster->y;
  rock->dx=shipx-monster->x;
  if (!rock->dy&&!rock->dx) {
    rock->dy=1;
    rock->dx=0;
  }
  float distance=sqrtf((float)rock->dx*(float)rock->dx+(float)rock->dy*(float)rock->dy);
  float adjust=rockspeed/distance;
  rock->dx=rock->dx*adjust;
  rock->dy=rock->dy*adjust;
  if (!rock->dy&&!rock->dx) {
    rock->dy=rockspeed;
    rock->dx=0;
  }
}

/* Update rock.
 */
 
static void rock_update(struct rock *rock) {
  rock->x+=rock->dx;
  rock->y+=rock->dy;
  if (
    (rock->x<ROCK_X_MIN)||
    (rock->y<ROCK_Y_MIN)||
    (rock->x>ROCK_X_MAX)||
    (rock->y>ROCK_Y_MAX)
  ) {
    rock->tileid=0;
    return;
  }
  if (!alive) return;
  int16_t dx=shipx-rock->x;
  int16_t dy=59*MM_PER_PIXEL-rock->y;
  int16_t threshold=MM_PER_PIXEL*4;
  if (dx>threshold) return;
  if (dy>threshold) return;
  if (dx<-threshold) return;
  if (dy<-threshold) return;
  sound_effect_hero_dead();
  alive=0;
  rock->tileid=0;
  game_over_delay=GAME_OVER_DELAY;
  explodeclock=5;
  explodeframe=0;
}

/* Rewrite the entire set of monsters.
 */
 
static void spawn_monsters(uint8_t waveid) {

  memset(monsterv,0,sizeof(monsterv));
  monsterdx=1;
  
  uint8_t rankc=0,filec=0,tileid=0;
  switch (waveid&7) { // We have 8 monster tiles; cycle through them.
    case 0: tileid=0x24; break; // bee
    case 1: tileid=0x2c; break; // ufo
    case 2: tileid=0x3c; break; // bat
    case 3: tileid=0x28; break; // wasp
    case 4: tileid=0x10; break; // broccoli
    case 5: tileid=0x38; break; // cream puff
    case 6: tileid=0x34; break; // jellyfish
    case 7: tileid=0x30; break; // evil twin
  }
  // phalanx size and rock timing advance progressively, until holding steady after wave 100.
  // (until the whole thing resets after 255, but i expect the player will have gotten bored by then).
       if (waveid<  4) { rankc=2; filec=3; rockspeed= 50; rocktimelo=60; rocktimehi=150; }
  else if (waveid<  8) { rankc=3; filec=3; rockspeed= 52; rocktimelo=60; rocktimehi=145; }
  else if (waveid< 12) { rankc=3; filec=3; rockspeed= 54; rocktimelo=59; rocktimehi=140; }
  else if (waveid< 16) { rankc=3; filec=3; rockspeed= 56; rocktimelo=58; rocktimehi=135; }
  else if (waveid< 20) { rankc=3; filec=3; rockspeed= 58; rocktimelo=57; rocktimehi=130; }
  else if (waveid< 24) { rankc=3; filec=3; rockspeed= 60; rocktimelo=56; rocktimehi=125; }
  else if (waveid< 28) { rankc=3; filec=3; rockspeed= 62; rocktimelo=55; rocktimehi=120; }
  else if (waveid< 32) { rankc=3; filec=4; rockspeed= 64; rocktimelo=54; rocktimehi=115; }
  else if (waveid< 36) { rankc=3; filec=4; rockspeed= 66; rocktimelo=53; rocktimehi=110; }
  else if (waveid< 40) { rankc=3; filec=4; rockspeed= 68; rocktimelo=52; rocktimehi=105; }
  else if (waveid< 44) { rankc=3; filec=4; rockspeed= 70; rocktimelo=51; rocktimehi=100; }
  else if (waveid< 48) { rankc=3; filec=4; rockspeed= 73; rocktimelo=50; rocktimehi= 95; }
  else if (waveid< 52) { rankc=3; filec=4; rockspeed= 76; rocktimelo=49; rocktimehi= 90; }
  else if (waveid< 56) { rankc=3; filec=4; rockspeed= 79; rocktimelo=48; rocktimehi= 85; }
  else if (waveid< 60) { rankc=3; filec=4; rockspeed= 82; rocktimelo=47; rocktimehi= 80; }
  else if (waveid< 64) { rankc=3; filec=5; rockspeed= 85; rocktimelo=46; rocktimehi= 75; }
  else if (waveid< 68) { rankc=3; filec=5; rockspeed= 88; rocktimelo=45; rocktimehi= 70; }
  else if (waveid< 72) { rankc=3; filec=5; rockspeed= 91; rocktimelo=44; rocktimehi= 65; }
  else if (waveid< 76) { rankc=3; filec=5; rockspeed= 94; rocktimelo=43; rocktimehi= 60; }
  else if (waveid< 80) { rankc=3; filec=5; rockspeed= 97; rocktimelo=42; rocktimehi= 55; }
  else if (waveid< 84) { rankc=3; filec=5; rockspeed=100; rocktimelo=41; rocktimehi= 50; }
  else if (waveid< 88) { rankc=4; filec=5; rockspeed=103; rocktimelo=40; rocktimehi= 50; }
  else if (waveid< 92) { rankc=4; filec=5; rockspeed=106; rocktimelo=39; rocktimehi= 50; }
  else if (waveid< 96) { rankc=4; filec=6; rockspeed=109; rocktimelo=38; rocktimehi= 50; }
  else if (waveid<100) { rankc=4; filec=6; rockspeed=112; rocktimelo=37; rocktimehi= 50; }
  else                 { rankc=4; filec=6; rockspeed=120; rocktimelo=36; rocktimehi= 50; }
  if (!rankc||!filec||!tileid) return;
  
  int16_t w=rankc*10*MM_PER_PIXEL;
  int16_t offsetx=rand()%(FBW_MM-w);
  monsterdx=(rand()&1)?-1:1;

  int16_t offsety=5*MM_PER_PIXEL;
  int16_t lead_length=rankc*10*MM_PER_PIXEL+offsety;
  monsterdyc=60;
  monsterdy=lead_length/monsterdyc;
  
  struct monster *monster=monsterv;
  uint8_t rank=0;
  for (;rank<rankc;rank++) {
    uint8_t file=0;
    for (;file<filec;file++,monster++) {
      monster->tileid=tileid;
      monster->x=file*10*MM_PER_PIXEL+offsetx;
      monster->y=rank*10*MM_PER_PIXEL+offsety-lead_length;
      monster->radius=4*MM_PER_PIXEL;
    }
  }
}

/* Rewrite the entire set of stars, randomly.
 */
 
static void reset_stars() {
  struct star *star=starv;
  uint8_t i=STAR_LIMIT;
  for (;i-->0;star++) {
    star->x=rand()%FBW_MM;
    star->y=rand()%FBH_MM;
    star->pixel=0; // signal to generate when we have a framebuffer
    star->dy=STAR_SPEED_MIN+rand()%(STAR_SPEED_MAX-STAR_SPEED_MIN);
  }
}

static void star_replace(struct star *star) {
  star->x=rand()%FBW_MM;
  star->y=0;
  star->pixel=0;
  star->dy=STAR_SPEED_MIN+rand()%(STAR_SPEED_MAX-STAR_SPEED_MIN);
}

/* Init.
 */

static int8_t _invaders_init() {
  if (gamek_image_decode(&img_aliensprites,aliensprites,0x7fffffff)<0) return -1;
  sound_effects_setup();
  gamek_platform_play_song(defend_the_galaxy,0xffff);
  load_hiscore();
  reset_stars();
  explodeframe=6;
  int16_t msgw=gamek_font_render_string(0,0,0,0,font_g06,message,messagec);
  messagex=(FBW_PIXELS>>1)-(msgw>>1);
  messagey=(FBH_PIXELS>>1)-3;
  return 0;
}

/* Fireworks.
 */
 
static void generate_fireworks(int16_t xpx,int16_t ypx,int16_t score) {

  // Find an available slot. Drop an old one if we need to.
  struct firework *firework=0;
  struct firework *eldest=fireworkv;
  struct firework *q=fireworkv;
  uint8_t i=FIREWORK_LIMIT;
  for (;i-->0;q++) {
    if (!q->msgc) {
      firework=q;
      break;
    }
    if (q->ttl<eldest->ttl) eldest=q;
  }
  if (!firework) firework=eldest;
  
  // TODO this is not a good substitute for snprintf :(
  if (score<0) score=0;
  if (score>=10000) firework->msgc=5;
  else if (score>=1000) firework->msgc=4;
  else if (score>=100) firework->msgc=3;
  else if (score>=10) firework->msgc=2;
  else firework->msgc=1;
  i=firework->msgc;
  for (;i-->0;score/=10) firework->msg[i]='0'+score%10;
  
  firework->y=ypx-2;
  firework->x=xpx-(firework->msgc*4-1)/2;
  firework->ttl=FIREWORK_TTL;
}

/* Lasers.
 */
 
static void laser_update(struct laser *laser) {
  laser->y-=LASER_SPEED;
  if (laser->h<LASER_H_TARGET) laser->h+=LASER_SPEED;
  if (laser->y<-laser->h) {
    // off screen, terminate and drop the combo
    sound_effect_combo_lost();
    laser->h=0; 
    points_kill=POINTS_KILL_INITIAL;
  }
}

static void fire_laser() {
  struct laser *laser=0;
  struct laser *q=laserv;
  uint8_t i=LASER_LIMIT;
  for (;i-->0;q++) {
    if (!q->h) {
      laser=q;
      break;
    }
  }
  if (!laser) return; // no available slots, drop it
  sound_effect_laser();
  laser->x=shipx;
  laser->y=55*MM_PER_PIXEL;
  laser->h=1;
}

/* Check one monster against all lasers.
 */
 
static struct laser *monster_should_die(const struct monster *monster) {
  struct laser *laser=laserv;
  uint8_t i=LASER_LIMIT;
  for (;i-->0;laser++) {
    if (!laser->h) continue;
    int16_t dx=laser->x-monster->x;
    if (dx>=monster->radius) continue;
    if (dx<=-monster->radius) continue;
    if (laser->y>=monster->y+monster->radius) continue;
    if (laser->y+laser->h<=monster->y-monster->radius) continue;
    return laser;
  }
  return 0;
}

/* Begin a new state. Either the hero has died, or all monsters are dead.
 */
 
static void begin_new_state() {

  // If the hero is dead, it's "game over" and show the high scores.
  if (!alive) {
    gamek_platform_play_song(defend_the_galaxy,0xffff);
    game_state=GAME_STATE_GAME_OVER;
    memcpy(message,"GAME OVER",9);
    messagec=9;
    int16_t msgw=gamek_font_render_string(0,0,0,0,font_g06,message,messagec);
    messagex=(FBW_PIXELS>>1)-(msgw>>1);
    messagey=(FBH_PIXELS>>1)-3;
    
    if (score==hiscore) {
      save_hiscore();
    }
    
  } else switch (game_state) {
  
    case GAME_STATE_PLAY: {
        sound_effect_end_wave();
        game_state=GAME_STATE_END_WAVE;
        game_over_delay=END_WAVE_DELAY;
        memcpy(message,"Wave ",5);
        if (current_wave>=100) {
          message[5]='0'+current_wave/100;
          message[6]='0'+(current_wave/10)%10;
          message[7]='0'+current_wave%10;
          memcpy(message+8," complete",9);
          messagec=17;
        } else if (current_wave>=10) {
          message[5]='0'+current_wave/10;
          message[6]='0'+current_wave%10;
          memcpy(message+7," complete",9);
          messagec=16;
        } else {
          message[5]='0'+current_wave;
          memcpy(message+6," complete",9);
          messagec=15;
        }
        int16_t msgw=gamek_font_render_string(0,0,0,0,font_g06,message,messagec);
        messagex=(FBW_PIXELS>>1)-(msgw>>1);
        messagey=(FBH_PIXELS>>1)-3;
      } break;
      
    case GAME_STATE_END_WAVE: {
        game_state=GAME_STATE_PLAY;
        current_wave++;
        spawn_monsters(current_wave);
        randomize_rock_time();
        messagec=0;
      } break;
      
    default: game_state=GAME_STATE_GAME_OVER;
  }
}

static void reset_game() {
  gamek_platform_play_song(supernova,0xffff);
  sound_effect_start_game();
  alive=1;
  current_wave=0;
  score=0;
  points_kill=POINTS_KILL_INITIAL;
  messagec=0;
  game_state=GAME_STATE_PLAY;
  game_over_delay=0;
  spawn_monsters(current_wave);
  randomize_rock_time();
  drop_missiles();
}

/* Update.
 */

static void _invaders_update() {
  uint8_t i;

  // Shared animation clock.
  if (animclock) animclock--;
  else {
    animclock=8;
    if (++animframe>=4) animframe=0;
  }
  
  // Stars.
  struct star *star=starv;
  for (i=STAR_LIMIT;i-->0;star++) {
    star->y+=star->dy;
    if (star->y>=FBH_MM) star_replace(star);
  }
  
  // If game is over, tick that clock, and advance state when it strikes zero.
  // This is the wave-to-wave state transition too, not just "game over".
  if (game_over_delay) {
    game_over_delay--;
    if (!game_over_delay) {
      begin_new_state();
      return;
    }
  }
  
  // Ship movement.
  if (alive) {
    if (shipdx) {
      shipspeed+=SHIP_ACCELERATION;
      if (shipspeed>SHIP_SPEED_MAX_MM) shipspeed=SHIP_SPEED_MAX_MM;
    } else {
      shipspeed-=SHIP_DECELERATION;
      if (shipspeed<0) shipspeed=0;
    }
    if (shipspeed) {
      if (shipdx) shipdxsticky=shipdx;
      shipx+=shipdxsticky*shipspeed;
      if (shipx<SHIP_LEFT_BOUND) shipx=SHIP_LEFT_BOUND;
      else if (shipx>SHIP_RIGHT_BOUND) shipx=SHIP_RIGHT_BOUND;
    }
    
  // Explosion animation.
  } else {
    if (explodeclock) explodeclock--;
    else {
      explodeclock=4;
      if (explodeframe<6) explodeframe++;
    }
  }
  
  // Lasers.
  struct laser *laser=laserv;
  for (i=LASER_LIMIT;i-->0;laser++) {
    if (!laser->h) continue;
    laser_update(laser);
  }
  
  // Throw a rock?
  if (rocktime) rocktime--;
  else {
    rock_generate();
    randomize_rock_time();
  }
  
  // Rocks.
  struct rock *rock=rockv;
  for (i=ROCK_LIMIT;i-->0;rock++) {
    if (!rock->tileid) continue;
    rock_update(rock);
  }
  
  // Monsters.
  int8_t breach=0,alldead=1;
  struct monster *monster=monsterv;
  for (i=MONSTER_LIMIT;i-->0;monster++) {
    if (!monster->tileid) continue;
    if (laser=monster_should_die(monster)) {
      sound_effect_monster_dead(points_kill);
      generate_fireworks(monster->x>>MM_PER_PIXEL_BITS,monster->y>>MM_PER_PIXEL_BITS,points_kill);
      monster->tileid=0;
      laser->h=0;
      score+=points_kill;
      if (score>hiscore) hiscore=score;
      points_kill+=POINTS_KILL_INCREMENT;
      if (points_kill>POINTS_KILL_MAX) points_kill=POINTS_KILL_MAX;
    }
    alldead=0;
    monster->y+=monsterdy;
    monster->x+=monsterdx*MONSTER_SPEED;
    if (monster->x<MONSTER_LEFT_BOUND) breach=-1;
    else if (monster->x>MONSTER_RIGHT_BOUND) breach=1;
  }
  if (monsterdyc) monsterdyc--;
  else monsterdy=0;
  if ((breach<0)&&(monsterdx<0)) monsterdx=1;
  else if ((breach>0)&&(monsterdx>0)) monsterdx=-1;
  if (alldead&&(game_state!=GAME_STATE_GAME_OVER)) {
    if (!game_over_delay) {
      game_over_delay=GAME_OVER_DELAY;
    }
  }
  
  // Fireworks.
  struct firework *firework=fireworkv;
  for (i=FIREWORK_LIMIT;i-->0;firework++) {
    if (!(firework->ttl%10)) firework->y--; // move up slowly
    if (!firework->ttl--) firework->msgc=0;
  }
}

/* Render scene.
 */

static uint8_t _invaders_render(struct gamek_image *fb) {
  uint8_t i;

  // Background.
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,gamek_image_pixel_from_rgb(fb->fmt,0,0,0));
  
  // Stars.
  struct star *star=starv;
  for (i=STAR_LIMIT;i-->0;star++) {
    if (!star->pixel) {
      uint8_t luma=((star->dy-STAR_SPEED_MIN)*255)/(STAR_SPEED_MAX-STAR_SPEED_MIN+1);
      star->pixel=gamek_image_pixel_from_rgb(fb->fmt,luma,luma,luma);
    }
    gamek_image_fill_rect(fb,star->x>>MM_PER_PIXEL_BITS,star->y>>MM_PER_PIXEL_BITS,1,1,star->pixel);
  }
  
  // Monsters.
  struct monster *monster=monsterv;
  for (i=MONSTER_LIMIT;i-->0;monster++) {
    if (!monster->tileid) continue;
    int16_t srcx=((monster->tileid&0x0f)+animframe)*8;
    int16_t srcy=(monster->tileid>>4)*8;
    int16_t dstx=(monster->x>>MM_PER_PIXEL_BITS)-4;
    int16_t dsty=(monster->y>>MM_PER_PIXEL_BITS)-4;
    gamek_image_blit(fb,dstx,dsty,&img_aliensprites,srcx,srcy,8,8,0);
  }
  
  // Lasers.
  uint32_t lasercolor=gamek_image_pixel_from_rgba(fb->fmt,0xff,0xff,0x00,0xff);//TODO animate laser color
  struct laser *laser=laserv;
  for (i=LASER_LIMIT;i-->0;laser++) {
    if (!laser->h) continue;
    gamek_image_fill_rect(fb,laser->x>>MM_PER_PIXEL_BITS,laser->y>>MM_PER_PIXEL_BITS,1,laser->h>>MM_PER_PIXEL_BITS,lasercolor);
  }
  
  // Rocks.
  struct rock *rock=rockv;
  for (i=ROCK_LIMIT;i-->0;rock++) {
    if (!rock->tileid) continue;
    int16_t srcx=((rock->tileid&0x0f)+animframe)*8;
    int16_t srcy=(rock->tileid>>4)*8;
    int16_t dstx=(rock->x>>MM_PER_PIXEL_BITS)-4;
    int16_t dsty=(rock->y>>MM_PER_PIXEL_BITS)-4;
    gamek_image_blit(fb,dstx,dsty,&img_aliensprites,srcx,srcy,8,8,0);
  }
  
  // Ship.
  if (alive) {
    uint8_t shipframe=0;
    if (shipdx<0) shipframe=1;
    else if (shipdx>0) shipframe=2;
    gamek_image_blit(fb,(shipx>>MM_PER_PIXEL_BITS)-4,55,&img_aliensprites,8*shipframe,0,8,8,0);
    
  // Explosion.
  } else if (explodeframe<6) {
    gamek_image_blit(fb,(shipx>>MM_PER_PIXEL_BITS)-8,51,&img_aliensprites,32+16*explodeframe,0,16,16,0);
  }
  
  // Fireworks.
  struct firework *firework=fireworkv;
  for (i=FIREWORK_LIMIT;i-->0;firework++) {
    if (!firework->msgc) continue;
    if (!(firework->ttl&1)) continue; // blink fast
    gamek_font_render_string(fb,firework->x,firework->y,0xffffffff,font_digits_3x5,firework->msg,firework->msgc);
  }
  
  // Score.
  char scorestr[6]={
    '0'+(score/100000)%10,
    '0'+(score/ 10000)%10,
    '0'+(score/  1000)%10,
    '0'+(score/   100)%10,
    '0'+(score/    10)%10,
    '0'+(score/     1)%10,
  };
  gamek_font_render_string(fb,FBW_PIXELS-24,1,0xffffffff,font_digits_3x5,scorestr,6);
  scorestr[0]='0'+(hiscore/100000)%10;
  scorestr[1]='0'+(hiscore/ 10000)%10;
  scorestr[2]='0'+(hiscore/  1000)%10;
  scorestr[3]='0'+(hiscore/   100)%10;
  scorestr[4]='0'+(hiscore/    10)%10;
  scorestr[5]='0'+(hiscore/     1)%10;
  if (!hiscore_color) hiscore_color=gamek_image_pixel_from_rgba(fb->fmt,0x80,0x80,0x80,0xff);
  gamek_font_render_string(fb,FBW_PIXELS-24,7,hiscore_color,font_digits_3x5,scorestr,6);
  
  // Message if present.
  if (messagec) {
    gamek_font_render_string(fb,messagex,messagey,0xffffffff,font_g06,message,messagec);
  }
  
  return 1;
}

/* Input.
 */

static void _invaders_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {

  // This is a one-player game absolutely. So we ignore the concept of players, and use the aggregate state.
  if (playerid) return;
  
  switch (btnid) {
    case GAMEK_BUTTON_LEFT: if (value) {
        shipdx=-1;
      } else if (shipdx<0) {
        shipdx=0;
      } break;
    case GAMEK_BUTTON_RIGHT: if (value) {
        shipdx=1;
      } else if (shipdx>0) {
        shipdx=0;
      } break;
    case GAMEK_BUTTON_SOUTH: if (value) switch (game_state) {
        case GAME_STATE_PLAY: if (alive) fire_laser(); break;
        case GAME_STATE_END_WAVE: break;
        case GAME_STATE_GAME_OVER: reset_game(); break;
      } break;
  }
}

/* Client definition.
 */

const struct gamek_client gamek_client={
  .title="invaders",
  .fbw=FBW_PIXELS,
  .fbh=FBH_PIXELS,
  .init=_invaders_init,
  .update=_invaders_update,
  .render=_invaders_render,
  .input_event=_invaders_input_event,
};
