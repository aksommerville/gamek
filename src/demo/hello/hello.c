#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const uint8_t font_g06[];
extern const uint8_t tangled_vine[]; // song

/* See etc/doc/mynth.txt
  00000000                   End of song.
  0nnnnnnn                   Delay (n) ticks.
  1000nnnn nnnnnnnn          Delay (n) ticks.
  1001cccc NNNNNNNv vvDDDDDD One-shot note. (c) chid, (N) noteid, (v) velocity, (D) duration ticks.
  1010cccc 0nnnnnnn 0vvvvvvv Note On. (c) chid, (n) noteid, (v) velocity.
  1011cccc 0nnnnnnn          Note Off.
  1100cccc kkkkkkkk vvvvvvvv Control Change. (k,v) normally in 0..127, but we do allow the full 8 bits.
  1101dddd vvvvvvvv          Pitch wheel. unsigned: 0x80 is default.
  111xxxxx ...               Reserved, illegal.
*/
static const uint8_t mysong[]={
  0xf0, // tempo ms/tick. 0xf0 is unusually coarse
  0x04, // start position, should be 4.
  0x00,0x04, // loop position, should also be 4.
  
  0x90,0x81,0x01,
  0x91,0x77,0x05,
  0x01,
  0x90,0x87,0x01,
  0x01,
  0x90,0x8f,0x01,
  0x02,
  0x90,0x95,0x01,
  0x02,
  
  0x00,
};

static const uint8_t smile_pixels[8*8*4]={
#define _ 0,0,0,0,
#define K 0,0,0,255,
#define W 255,255,255,255,
#define L 255,255,0,255,
  _ _ K K K K _ _
  _ K L L L L K _
  K L L L K K L K
  K L L L K K L K
  K L L L L L L K
  K L L L K K K K
  _ K L L L L K _
  _ _ K K K K _ _
#undef _
#undef K
#undef W
#undef L
};
struct gamek_image smile={
  .v=(void*)smile_pixels,
  .w=8,
  .h=8,
  .stride=8<<2,
  .fmt=GAMEK_IMGFMT_RGBX,
  .flags=GAMEK_IMAGE_FLAG_TRANSPARENT,
};

static uint8_t cursor_visible=0;
static uint16_t inputv[5]={0};

/* Init.
 */
 
static int16_t mywave[512];
 
static int8_t _hello_init() {

  memset(mywave,0x40,512);
  memset(mywave+256,0xc0,512);
  gamek_platform_set_audio_wave(0,mywave,512);
  
  /* This stuff is specific to Mynth.
   * We might need to get weird when the other synthesizers are implemented.
   */
  gamek_platform_audio_event(0,0xb0,0x0c,0x20); // atkclo
  gamek_platform_audio_event(0,0xb0,0x2c,0x04); // atkchi
  gamek_platform_audio_event(0,0xb0,0x0e,0x28); // decclo
  gamek_platform_audio_event(0,0xb0,0x2e,0x06); // decchi
  gamek_platform_audio_event(0,0xb0,0x0d,0x30); // atkvlo
  gamek_platform_audio_event(0,0xb0,0x2d,0xa0); // atkvhi
  gamek_platform_audio_event(0,0xb0,0x0f,0x10); // susvlo
  gamek_platform_audio_event(0,0xb0,0x2f,0x30); // susvhi
  gamek_platform_audio_event(0,0xb0,0x10,0x08); // rlsclo (8ms intervals)
  gamek_platform_audio_event(0,0xb0,0x30,0x10); // rlschi
  gamek_platform_audio_event(0,0xb0,0x40,0x40); // enable sustain
  gamek_platform_audio_event(0,0xb0,0x46,0x00); // wave
  gamek_platform_audio_event(0,0xb0,0x47,0x14); // wheel range
  gamek_platform_audio_event(0,0xb0,0x48,0x01); // warble range
  gamek_platform_audio_event(0,0xb0,0x49,0x02); // warble rate
  /**/
  
  //gamek_platform_play_song(mysong,sizeof(mysong));
  gamek_platform_play_song(tangled_vine,0xffff);

  return 0;
}

/* Update.
 */
 
static void _hello_update() {
}

/* Render.
 */

static void hello_draw_input(struct gamek_image *fb,int16_t x,int16_t y,uint16_t input) {
  //TODO Yes, pixel-from-rgba is dirt cheap, but it would still make sense to cache all these colors somewhere.
  // The framebuffer pixel format is not going to change after the first frame.
  uint32_t color_bg_on=gamek_image_pixel_from_rgba(fb->fmt,0x80,0x40,0x00,0xff);
  uint32_t color_bg_off=gamek_image_pixel_from_rgba(fb->fmt,0x60,0x60,0x60,0xff);
  uint32_t color_button_on=gamek_image_pixel_from_rgba(fb->fmt,0xff,0xff,0xff,0xff);
  uint32_t color_button_off=gamek_image_pixel_from_rgba(fb->fmt,0x00,0x00,0x00,0xff);
  gamek_image_fill_rect(fb,x   ,y   ,13,7,(input&GAMEK_BUTTON_CD)?color_bg_on:color_bg_off);
  gamek_image_fill_rect(fb,x+ 1,y+ 1, 1,1,(input&GAMEK_BUTTON_L1)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 3,y+ 1, 1,1,(input&GAMEK_BUTTON_L2)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+11,y+ 1, 1,1,(input&GAMEK_BUTTON_R1)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 9,y+ 1, 1,1,(input&GAMEK_BUTTON_R2)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 1,y+ 4, 1,1,(input&GAMEK_BUTTON_LEFT)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 2,y+ 3, 1,1,(input&GAMEK_BUTTON_UP)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 2,y+ 5, 1,1,(input&GAMEK_BUTTON_DOWN)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 3,y+ 4, 1,1,(input&GAMEK_BUTTON_RIGHT)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 9,y+ 4, 1,1,(input&GAMEK_BUTTON_WEST)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+10,y+ 3, 1,1,(input&GAMEK_BUTTON_NORTH)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+10,y+ 5, 1,1,(input&GAMEK_BUTTON_SOUTH)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+11,y+ 4, 1,1,(input&GAMEK_BUTTON_EAST)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 7,y+ 4, 1,1,(input&GAMEK_BUTTON_AUX1)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 5,y+ 4, 1,1,(input&GAMEK_BUTTON_AUX2)?color_button_on:color_button_off);
  gamek_image_fill_rect(fb,x+ 6,y+ 2, 1,1,(input&GAMEK_BUTTON_AUX3)?color_button_on:color_button_off);
}

static int16_t textx=2;
static int16_t textdx=1;
 
static uint8_t _hello_render(struct gamek_image *fb) {
  gamek_image_clear(fb);
  gamek_image_fill_rect(fb,1,1,fb->w-2,fb->h-2,gamek_image_pixel_from_rgba(fb->fmt,0x80,0x80,0x80,0xff));
  
  int16_t x=2;
  uint8_t xform=0;
  for (;xform<8;xform++,x+=smile.w+1) {
    gamek_image_blit(fb,x,2,&smile,0,0,smile.w,smile.h,xform);
  }
  
  const uint16_t *input=inputv;
  int i=5;
  for (x=2;i-->0;input++,x+=14) {
    hello_draw_input(fb,x,11,*input);
  }
  
  gamek_font_render_string(fb,textx+ 0,19,gamek_image_pixel_from_rgba(fb->fmt,0xff,0x00,0x00,0xff),font_g06,"RED",3);
  gamek_font_render_string(fb,textx+16,19,gamek_image_pixel_from_rgba(fb->fmt,0x00,0xff,0x00,0xff),font_g06,"GREEN",5);
  gamek_font_render_string(fb,textx+43,19,gamek_image_pixel_from_rgba(fb->fmt,0x00,0x00,0xff,0xff),font_g06,"BLUE",4);
  textx+=textdx;
  if ((textx<2)&&(textdx<0)) textdx=-textdx;
  else if ((textx>30)&&(textdx>0)) textdx=-textdx;
  
  gamek_font_render_string(fb,2,28,gamek_image_pixel_from_rgba(fb->fmt,0x00,0x00,0x00,0xff),font_g06,"Welcome to gamek!",-1);
  char msg[256];
  int msgc=snprintf(msg,sizeof(msg),"Platform: %s",gamek_platform_details.name);
  if ((msgc>0)&&(msgc<sizeof(msg))) gamek_font_render_string(fb,2,37,gamek_image_pixel_from_rgba(fb->fmt,0x00,0x80,0x00,0xff),font_g06,msg,msgc);
  
  return 1;
}

/* Mapped player input.
 */
 
static void _hello_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
  //fprintf(stderr,"%s %d.%04x=%d\n",__func__,playerid,btnid,value);
  if (playerid<5) {
    if (value) inputv[playerid]|=btnid;
    else inputv[playerid]&=~btnid;
  }
  
  // TEMP? Play tones on input.
  switch (playerid) {
    case 1: switch (btnid) {
        case GAMEK_BUTTON_UP:    gamek_platform_audio_event(0,value?0x90:0x80,0x40,0x40); break;
        case GAMEK_BUTTON_DOWN:  gamek_platform_audio_event(0,value?0x90:0x80,0x42,0x40); break;
        case GAMEK_BUTTON_LEFT:  gamek_platform_audio_event(0,value?0x90:0x80,0x43,0x40); break;
        case GAMEK_BUTTON_RIGHT: gamek_platform_audio_event(0,value?0x90:0x80,0x45,0x40); break;
        case GAMEK_BUTTON_SOUTH: gamek_platform_audio_event(0,value?0x90:0x80,0x47,0x40); break;
        case GAMEK_BUTTON_WEST:  gamek_platform_audio_event(0,value?0x90:0x80,0x48,0x40); break;
        case GAMEK_BUTTON_EAST:  gamek_platform_audio_event(0,value?0x90:0x80,0x4a,0x40); break;
        case GAMEK_BUTTON_NORTH: gamek_platform_audio_event(0,value?0x90:0x80,0x4c,0x40); break;
      } break;
  }
}

/* MIDI input.
 */
 
static void _hello_midi_in(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  //fprintf(stderr,"%s[%02x] %02x %02x %02x\n",__func__,chid,opcode,a,b);
  gamek_platform_audio_event(chid,opcode,a,b);
}

/* Client definition.
 */

const struct gamek_client gamek_client={
  .title="hello",
  .fbw=96,
  .fbh=64,

  // Most games should implement all of these:
  .init=_hello_init,
  .update=_hello_update,
  .render=_hello_render,
  .input_event=_hello_input_event,
  
  // MIDI input is usually not used:
  .midi_in=_hello_midi_in,
};
