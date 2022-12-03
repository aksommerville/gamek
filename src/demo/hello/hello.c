#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include "common/gamek_font.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const uint8_t font_g06[];

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
 
static int8_t _hello_init() {
  fprintf(stderr,"%s\n",__func__);
  return 0;
}

/* Update.
 */
 
static void _hello_update() {
  //fprintf(stderr,"%s\n",__func__);
}

/* Render.
 */
 
static uint32_t basecolor=0x80008000;

static void hello_draw_input(struct gamek_image *fb,int16_t x,int16_t y,uint16_t input) {
  gamek_image_fill_rect(fb,x   ,y   ,13,7,(input&GAMEK_BUTTON_CD)?0x00004080:0x60606060);
  gamek_image_fill_rect(fb,x+ 1,y+ 1, 1,1,(input&GAMEK_BUTTON_L1)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 3,y+ 1, 1,1,(input&GAMEK_BUTTON_L2)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+11,y+ 1, 1,1,(input&GAMEK_BUTTON_R1)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 9,y+ 1, 1,1,(input&GAMEK_BUTTON_R2)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 1,y+ 4, 1,1,(input&GAMEK_BUTTON_LEFT)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 2,y+ 3, 1,1,(input&GAMEK_BUTTON_UP)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 2,y+ 5, 1,1,(input&GAMEK_BUTTON_DOWN)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 3,y+ 4, 1,1,(input&GAMEK_BUTTON_RIGHT)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 9,y+ 4, 1,1,(input&GAMEK_BUTTON_WEST)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+10,y+ 3, 1,1,(input&GAMEK_BUTTON_NORTH)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+10,y+ 5, 1,1,(input&GAMEK_BUTTON_SOUTH)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+11,y+ 4, 1,1,(input&GAMEK_BUTTON_EAST)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 7,y+ 4, 1,1,(input&GAMEK_BUTTON_AUX1)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 5,y+ 4, 1,1,(input&GAMEK_BUTTON_AUX2)?0xffffffff:0x00000000);
  gamek_image_fill_rect(fb,x+ 6,y+ 2, 1,1,(input&GAMEK_BUTTON_AUX3)?0xffffffff:0x00000000);
}
 
static uint8_t _hello_render(struct gamek_image *fb) {
  gamek_image_clear(fb);
  gamek_image_fill_rect(fb,1,1,fb->w-2,fb->h-2,0x80808080);
  
  gamek_image_fill_rect(fb,2,2,10,fb->h-4,0xff000000);
  gamek_image_fill_rect(fb,13,2,10,fb->h-4,0x00ff0000);
  gamek_image_fill_rect(fb,24,2,10,fb->h-4,0x0000ff00);
  gamek_image_fill_rect(fb,35,2,10,fb->h-4,0x000000ff);
  
  int16_t x=46;
  uint8_t xform=0;
  for (;xform<8;xform++,x+=smile.w+1) {
    gamek_image_blit(fb,x,2,&smile,0,0,smile.w,smile.h,xform);
  }
  
  uint32_t glyph=0xf999f000;
  uint8_t glyphw=4,glyphh=5;
  uint32_t color=basecolor;
  basecolor+=0x01010101;
  uint32_t dcolor=0x04080204;
  uint8_t i=20;
  for (x=46;i-->0;x+=glyphw+1,color+=dcolor) {
    gamek_image_render_glyph(fb,x,11,glyph,glyphw,glyphh,color);
  }
  
  const uint16_t *input=inputv;
  for (i=5,x=46;i-->0;input++,x+=14) {
    hello_draw_input(fb,x,17,*input);
  }
  
  gamek_font_render_string(fb,46,25+font_g06[0]*0,0x00000000,font_g06,"Hello cruel world!",-1);
  gamek_font_render_string(fb,46,25+font_g06[0]*1,0xffffffff,font_g06,"the quick brown fox",-1);
  gamek_font_render_string(fb,46,25+font_g06[0]*2,0xffffffff,font_g06,"jumps over the lazy dog.",-1);
  gamek_font_render_string(fb,46,25+font_g06[0]*3,0xff00ff00,font_g06,"THE QUICK BROWN FOX",-1);
  gamek_font_render_string(fb,46,25+font_g06[0]*4,0xff00ff00,font_g06,"JUMPS OVER THE LAZY DOG!",-1);
  
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
}

/* Keyboard input.
 * It's unusual for a game to want this.
 * Keys may be mapped to other actions by the platform layer; you can't control that.
 * If eg "wasd" are mapped to a d-pad, you won't be able to receive them as text.
 * If you take text input, do it console style: Present a table of letters for the user to pick from.
 */
 
static uint8_t _hello_keyboard_input(uint32_t keycode,uint8_t value) {
  //fprintf(stderr,"%s %08x=%d\n",__func__,keycode,value);
  return 0; // nonzero to ack
}

static void _hello_text_input(uint32_t codepoint) {
  fprintf(stderr,"%s U+%x\n",__func__,codepoint);
  if (codepoint=='6') {
    fprintf(stderr,"Terminating because you typed 6.\n");
    gamek_platform_terminate(0);
  }
}

/* Mouse events.
 * Like keyboard, this is exotic for gamek games.
 * Not all supported platforms can even supply mouse events.
 */
 
static void _hello_mouse_motion(int16_t x,int16_t y) {
  //fprintf(stderr,"%s %d,%d\n",__func__,x,y);
}

static void _hello_mouse_button(uint8_t btnid,uint8_t value) {
  fprintf(stderr,"%s %d.%d\n",__func__,btnid,value);
  if ((btnid==1)&&value) {
    if (cursor_visible) {
      gamek_platform_show_cursor(0);
      cursor_visible=0;
    } else {
      gamek_platform_show_cursor(1);
      cursor_visible=1;
    }
  }
}

static void _hello_mouse_wheel(int16_t dx,int16_t dy) {
  fprintf(stderr,"%s %+d,%+d\n",__func__,dx,dy);
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

  // Most games should implement all of these:
  .init=_hello_init,
  .update=_hello_update,
  .render=_hello_render,
  .input_event=_hello_input_event,
  
  // Keyboards are optional. If you do text entry, it's good to support this:
  .keyboard_input=_hello_keyboard_input,
  .text_input=_hello_text_input,
  
  // Mouse input is usually not used:
  .mouse_motion=_hello_mouse_motion,
  .mouse_button=_hello_mouse_button,
  .mouse_wheel=_hello_mouse_wheel,
  
  // MIDI input is usually not used:
  .midi_in=_hello_midi_in,
};
