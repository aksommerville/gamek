#ifndef MYNTH_INTERNAL_H
#define MYNTH_INTERNAL_H

#include "mynth.h"
#include <stdio.h>
#include <math.h>

#define MYNTH_CHANNEL_LIMIT 16
#define MYNTH_VOICE_LIMIT 16 /* 1..255 */
#define MYNTH_WAVE_SIZE_BITS 9
#define MYNTH_WAVE_SIZE_SAMPLES (1<<9)
#define MYNTH_P_SHIFT (32-MYNTH_WAVE_SIZE_BITS)
#define MYNTH_WAVE_COUNT 8

// Envelope runner.
struct mynth_env {
  int32_t v; // normalized to 0..0x00ffffff
  int32_t c;
  int32_t d;
  struct mynth_env_point {
    int32_t v;
    int32_t c; // all nonzero at start. becomes zero as we cross this point
  } pointv[3];
  uint8_t hold;
  uint8_t sustain;
};

extern struct mynth {
  int32_t rate;
  
  const uint8_t *song;
  uint16_t songc;
  uint16_t songp;
  uint32_t songdelay; // frames
  uint32_t songtempo; // frames per tick
  uint8_t songstartp;
  uint16_t songloopp;
  
  struct mynth_channel {
    uint8_t volume; // 0..0x7f; controller 0x07
    uint16_t wheel; // 0..0x3fff, default 0x2000
    float bend; // rate multiplier, derived from (wheel). Definitely 1 in the default case 0x2000.
    uint8_t wheel_range; // dimes, default 20 (ie one full step)
    uint8_t waveid;
    const int16_t *wave;
    int32_t atkclo,atkvlo,decclo,susvlo,rlsclo; // envelope at minimum velocity
    int32_t atkchi,atkvhi,decchi,susvhi,rlschi; // ...maximum
    uint8_t sustain;
    uint8_t warble_range; // dimes
    uint8_t warble_rate; // hz
    uint8_t warble_phase;
  } channelv[MYNTH_CHANNEL_LIMIT];
  
  struct mynth_voice {
    uint8_t chid,noteid; // For identification. (0xff,0xff) if unaddressable
    uint32_t ttl; // Will stop after so many frames, or if zero run forever.
    const int16_t *v; // Wave, or null if voice not in use.
    uint32_t p;
    uint32_t dp;
    struct mynth_env env;
    uint32_t dplo,dphi;
    int32_t ddp;
  } voicev[MYNTH_VOICE_LIMIT];
  uint8_t voicec; // 0..MYNTH_VOICE_LIMIT
  
  uint32_t ratev[128]; // Master rate table.
  
  const int16_t *wavev[MYNTH_WAVE_COUNT];
} mynth;

void mynth_reset_all(); // retains most controller state
void mynth_reset_completely(); // return all controllers to initial state
void mynth_stop_voices_for_channel(uint8_t chid);
void mynth_release_voices_for_channel(uint8_t chid);
void mynth_release_all();
void mynth_note_once(uint8_t chid,uint8_t noteid,uint8_t velocity,uint32_t ttl);
struct mynth_voice *mynth_note_on(struct mynth_channel *channel,uint8_t a,uint8_t b);
void mynth_note_off(struct mynth_channel *channel,uint8_t a,uint8_t b);
void mynth_wheel(struct mynth_channel *channel,uint16_t v);

void mynth_channel_reset(struct mynth_channel *channel);
void mynth_channel_control(struct mynth_channel *channel,uint8_t k,uint8_t v);
void mynth_channel_generate_envelope(struct mynth_env *env,const struct mynth_channel *channel,uint8_t velocity);

/* At 'begin', a voice is completely uninitialized.
 * 'begin' must wipe it and assign everything needed, or make it unaddressable with TTL zero if not usable.
 */
void mynth_voice_update(int16_t *v,uint16_t c,struct mynth_voice *voice);
void mynth_voice_release(struct mynth_voice *voice);
void mynth_voice_begin(struct mynth_voice *voice,struct mynth_channel *channel,uint8_t noteid,uint8_t velocity);
void mynth_voice_reapply_wheel(struct mynth_voice *voice,struct mynth_channel *channel);

/* Process any events ready without a delay.
 * After completion, either (mynth.songdelay) is nonzero, or (mynth.song) has been dropped.
 */
void mynth_song_update();

#endif
