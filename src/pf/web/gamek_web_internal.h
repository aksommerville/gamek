#ifndef GAMEK_WEB_INTERNAL_H
#define GAMEK_WEB_INTERNAL_H

#include "pf/gamek_pf.h"
#include "common/gamek_image.h"

#define GAMEK_WEB_FB_LIMIT 256

extern struct gamek_web {
  struct gamek_image fb;
} gamek_web;

/* Hooks for the JS wrapper to call into.
 * (it's inconvenient to have JS look things up in gamek_client).
 */
int8_t gamek_web_client_init();
void gamek_web_client_update();
void *gamek_web_client_render();
void gamek_web_client_input_event(uint8_t playerid,uint16_t btnid,uint8_t value);
void gamek_web_client_midi_in(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b);

/* Implemented by JS wrapper for us to call.
 */
void gamek_web_external_set_title(const char *title); // it's inconvenient to read from a struct in JS
void gamek_web_external_set_framebuffer(void *v,int w,int h);
void gamek_web_external_console_log(const char *src);

#endif
