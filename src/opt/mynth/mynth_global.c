#include "mynth_internal.h"
#include <string.h>

#if GAMEK_PLATFORM_IS_TINY
  #include <avr/pgmspace.h>
#else
  #define PROGMEM
#endif

/* Globals and default state.
 */

static const int16_t mynth_wave_default[MYNTH_WAVE_SIZE_SAMPLES] PROGMEM={
  0,401,802,1203,1604,2005,2405,2805,3205,3604,4002,4400,4798,5194,5590,5985,6379,6772,7164,7555,
  7945,8334,8721,9107,9492,9875,10257,10637,11016,11393,11768,12142,12513,12883,13251,13617,13981,14342,
  14702,15059,15414,15767,16117,16465,16811,17154,17494,17832,18167,18499,18828,19155,19479,19800,20118,20432,
  20744,21053,21358,21660,21959,22255,22547,22836,23122,23404,23682,23957,24229,24496,24760,25020,25277,25530,
  25778,26023,26264,26501,26735,26964,27189,27409,27626,27839,28047,28251,28451,28647,28838,29025,29208,29386,29560,
  29729,29894,30055,30210,30362,30508,30650,30788,30921,31049,31173,31291,31406,31515,31620,31720,31815,31905,31990,
  32071,32147,32218,32284,32346,32402,32454,32500,32542,32579,32611,32638,32660,32677,32690,32697,32700,32697,32690,
  32677,32660,32638,32611,32579,32542,32500,32454,32402,32346,32284,32218,32147,32071,31990,31905,31815,31720,31620,
  31515,31406,31291,31173,31049,30921,30788,30650,30508,30362,30210,30055,29894,29729,29560,29386,29208,29025,28838,
  28647,28451,28251,28047,27839,27626,27409,27189,26964,26735,26501,26264,26023,25778,25530,25277,25020,24760,24496,
  24229,23957,23682,23404,23122,22836,22547,22255,21959,21660,21358,21053,20744,20432,20118,19800,19479,19155,18828,
  18499,18167,17832,17494,17154,16811,16465,16117,15767,15414,15059,14702,14342,13981,13617,13251,12883,12513,12142,
  11768,11393,11016,10637,10257,9875,9492,9107,8721,8334,7945,7555,7164,6772,6379,5985,5590,5194,4798,4400,4002,3604,
  3205,2805,2405,2005,1604,1203,802,401,0,-402,-803,-1204,-1605,-2006,-2406,-2806,-3206,-3605,-4003,-4401,-4799,-5195,
  -5591,-5986,-6380,-6773,-7165,-7556,-7946,-8335,-8722,-9108,-9493,-9876,-10258,-10638,-11017,-11394,-11769,-12143,
  -12514,-12884,-13252,-13618,-13982,-14343,-14703,-15060,-15415,-15768,-16118,-16466,-16812,-17155,-17495,-17833,-18168,
  -18500,-18829,-19156,-19480,-19801,-20119,-20433,-20745,-21054,-21359,-21661,-21960,-22256,-22548,-22837,-23123,-23405,
  -23683,-23958,-24230,-24497,-24761,-25021,-25278,-25531,-25779,-26024,-26265,-26502,-26736,-26965,-27190,-27410,-27627,
  -27840,-28048,-28252,-28452,-28648,-28839,-29026,-29209,-29387,-29561,-29730,-29895,-30056,-30211,-30363,-30509,-30651,
  -30789,-30922,-31050,-31174,-31292,-31407,-31516,-31621,-31721,-31816,-31906,-31991,-32072,-32148,-32219,-32285,-32347,
  -32403,-32455,-32501,-32543,-32580,-32612,-32639,-32661,-32678,-32691,-32698,-32700,-32698,-32691,-32678,-32661,-32639,
  -32612,-32580,-32543,-32501,-32455,-32403,-32347,-32285,-32219,-32148,-32072,-31991,-31906,-31816,-31721,-31621,-31516,
  -31407,-31292,-31174,-31050,-30922,-30789,-30651,-30509,-30363,-30211,-30056,-29895,-29730,-29561,-29387,-29209,-29026,
  -28839,-28648,-28452,-28252,-28048,-27840,-27627,-27410,-27190,-26965,-26736,-26502,-26265,-26024,-25779,-25531,-25278,
  -25021,-24761,-24497,-24230,-23958,-23683,-23405,-23123,-22837,-22548,-22256,-21960,-21661,-21359,-21054,-20745,-20433,
  -20119,-19801,-19480,-19156,-18829,-18500,-18168,-17833,-17495,-17155,-16812,-16466,-16118,-15768,-15415,-15060,-14703,
  -14343,-13982,-13618,-13252,-12884,-12514,-12143,-11769,-11394,-11017,-10638,-10258,-9876,-9493,-9108,-8722,-8335,-7946,
  -7556,-7165,-6773,-6380,-5986,-5591,-5195,-4799,-4401,-4003,-3605,-3206,-2806,-2406,-2006,-1605,-1204,-803,-402
};

struct mynth mynth={
  .rate=22050, // Reference rate for the initial rate table, hopefully also the default rate for Tiny builds.
  .ratev={
    2125742,2252146,2386065,2527948,2678268,2837526,3006254,3185015,3374406,3575058,3787642,4012867,4251485,
    4504291,4772130,5055896,5356535,5675051,6012507,6370030,6748811,7150117,7575285,8025735,8502970,9008582,
    9544261,10111792,10713070,11350103,12025015,12740059,13497623,14300233,15150569,16051469,17005939,18017165,
    19088521,20223584,21426141,22700205,24050030,25480119,26995246,28600467,30301139,32102938,34011878,36034330,
    38177043,40447168,42852281,45400411,48100060,50960238,53990491,57200933,60602276,64205876,68023757,72068660,
    76354085,80894335,85704563,90800821,96200119,101920476,107980983,114401866,121204555,128411753,136047513,
    144137319,152708170,161788671,171409126,181601643,192400238,203840952,215961966,228803732,242409110,256823506,
    272095026,288274639,305416341,323577341,342818251,363203285,384800477,407681904,431923931,457607465,484818220,
    513647012,544190053,576549277,610832681,647154683,685636503,726406571,769600953,815363807,863847862,915214929,
    969636441,1027294024,1088380105,1153098554,1221665363,1294309365,1371273005,1452813141,1539201906,1630727614,
    1727695724,1830429858,1939272882,2054588048,2176760211,2306197109,2443330725,2588618730,2742546010,2905626283,
    3078403812,3261455229,
  },
  .wavev={
    mynth_wave_default,
    mynth_wave_default,
    mynth_wave_default,
    mynth_wave_default,
    mynth_wave_default,
    mynth_wave_default,
    mynth_wave_default,
    mynth_wave_default,
  },
};

/* Replace entry in wave table.
 */
 
void mynth_set_wave(uint8_t waveid,const int16_t *v) {
  if (waveid>=MYNTH_WAVE_COUNT) return;
  if (!v) mynth.wavev[waveid]=mynth_wave_default;
  else mynth.wavev[waveid]=v;
}

/* Recalculate rate table.
 */
 
static void mynth_recalculate_rate_table(int32_t new_rate) {
  float adjust=(float)mynth.rate/(float)new_rate;
  uint32_t *v=mynth.ratev;
  uint8_t i=128;
  for (;i-->0;v++) {
    *v=(uint32_t)(((float)(*v))*adjust);
  }
  mynth.rate=new_rate;
}

/* Init.
 */
 
void mynth_init(int32_t rate) {
  if (rate>0) {
    if (rate!=mynth.rate) {
      mynth_recalculate_rate_table(rate);
    }
  }
  mynth_reset_all();
}

/* Update.
 */
 
static void mynth_update_voices(int16_t *v,uint16_t c) {
  struct mynth_voice *voice=mynth.voicev;
  uint8_t i=mynth.voicec;
  for (;i-->0;voice++) mynth_voice_update(v,c,voice);
}
 
void mynth_update(int16_t *v,uint16_t c) {
  memset(v,0,c<<1);
  if (mynth.song) {
    while (c>0) {
      if (!mynth.songdelay) {
        mynth_song_update();
        if (!mynth.song||!mynth.songdelay) {
          mynth_update_voices(v,c);
          return;
        }
      }
      uint32_t okframec=mynth.songdelay;
      if (okframec>c) okframec=c;
      mynth_update_voices(v,okframec);
      v+=okframec;
      c-=okframec;
      mynth.songdelay-=okframec;
    }
  } else {
    mynth_update_voices(v,c);
  }
  while (mynth.voicec&&!mynth.voicev[mynth.voicec-1].v) mynth.voicec--;
}

/* Full reset.
 */
 
void mynth_reset_all() {
  struct mynth_channel *channel=mynth.channelv;
  uint8_t i=MYNTH_CHANNEL_LIMIT;
  for (;i-->0;channel++) mynth_channel_reset(channel);
  mynth.voicec=0;
}

/* Silence or release one channel.
 */

void mynth_stop_voices_for_channel(uint8_t chid) {
  if (chid>=MYNTH_CHANNEL_LIMIT) return;
  struct mynth_voice *voice=mynth.voicev;
  uint8_t i=mynth.voicec;
  for (;i-->0;voice++) {
    if (voice->chid!=chid) continue;
    voice->v=0;
  }
}

void mynth_release_voices_for_channel(uint8_t chid) {
  if (chid>=MYNTH_CHANNEL_LIMIT) return;
  struct mynth_voice *voice=mynth.voicev;
  uint8_t i=mynth.voicec;
  for (;i-->0;voice++) {
    if (!voice->v) continue;
    if (voice->chid!=chid) continue;
    mynth_voice_release(voice);
  }
}

void mynth_release_all() {
  struct mynth_voice *voice=mynth.voicev;
  uint8_t i=mynth.voicec;
  for (;i-->0;voice++) {
    if (!voice->v) continue;
    mynth_voice_release(voice);
  }
}

/* Note Once.
 */
 
void mynth_note_once(uint8_t chid,uint8_t noteid,uint8_t velocity,uint32_t ttl) {
  if (chid>=MYNTH_CHANNEL_LIMIT) return;
  struct mynth_channel *channel=mynth.channelv+chid;
  struct mynth_voice *voice=mynth_note_on(channel,noteid,velocity);
  if (!voice) return;
  voice->ttl=ttl;
}

/* Note Off.
 */
 
void mynth_note_off(struct mynth_channel *channel,uint8_t a,uint8_t b) {
  uint8_t chid=channel-mynth.channelv;
  struct mynth_voice *voice=mynth.voicev;
  uint8_t i=mynth.voicec;
  for (;i-->0;voice++) {
    if (!voice->v) continue;
    if (voice->chid!=chid) continue;
    if (voice->noteid!=a) continue;
    mynth_voice_release(voice);
    // keep iterating, in case there's more than one
  }
}

/* Note On.
 */
 
struct mynth_voice *mynth_note_on(struct mynth_channel *channel,uint8_t a,uint8_t b) {
  
  if (!channel->wave) return 0;
  
  // Find an available voice, preferring to fill gaps before growing the list.
  struct mynth_voice *voice=0;
  struct mynth_voice *q=mynth.voicev;
  uint8_t i=mynth.voicec;
  for (;i-->0;q++) {
    if (q->v) continue;
    voice=q;
    break;
  }
  if (!voice) {
    if (mynth.voicec<MYNTH_VOICE_LIMIT) {
      voice=mynth.voicev+mynth.voicec++;
    } else {
      return 0;
    }
  }
  
  mynth_voice_begin(voice,channel,a,b);
  if (!voice->v) return 0;
  return voice;
}

/* Pitch Wheel.
 */
 
void mynth_wheel(struct mynth_channel *channel,uint16_t v) {
  if (v==channel->wheel) return;
  channel->wheel=v;
  channel->bend=powf(2.0f,(((int16_t)channel->wheel-0x2000)*channel->wheel_range)/(8192.0f*120.0f));
  uint8_t chid=channel-mynth.channelv;
  struct mynth_voice *voice=mynth.voicev;
  uint8_t i=mynth.voicec;
  for (;i-->0;voice++) {
    if (voice->chid!=chid) continue;
    mynth_voice_reapply_wheel(voice,channel);
  }
}

/* Event.
 */
 
void mynth_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  
  // (chid) 0xff is global. Look for system events.
  if (chid==0xff) {
    switch (opcode) {
      case 0xff: mynth_reset_all(); return;
    }
    return;
  }
  
  if (chid>=MYNTH_CHANNEL_LIMIT) return;
  struct mynth_channel *channel=mynth.channelv+chid;
  switch (opcode) {
    case 0x80: mynth_note_off(channel,a,b); return;
    case 0x90: mynth_note_on(channel,a,b); return;
    case 0xb0: mynth_channel_control(channel,a,b); return;
    case 0xe0: mynth_wheel(channel,a|(b<<7)); return;
  }
}

/* Play song.
 */
 
void mynth_play_song(const void *v,uint16_t c) {

  mynth_release_all();

  if (!v||(c<4)) { // null or too small to be a song: no song
    mynth.song=0;
    mynth.songc=0;
    return;
  }
  const uint8_t *src=v;
  uint8_t tempo=src[0];
  uint8_t startp=src[1];
  uint16_t loopp=(src[2]<<8)|src[3];
  if (!tempo||(startp<4)||(loopp<startp)||(loopp>c)) {
    mynth.song=0;
    mynth.songc=0;
    return;
  }
  
  mynth.songtempo=(mynth.rate*tempo)/1000;
  if (mynth.songtempo<1) mynth.songtempo=1;
  mynth.songdelay=0;
  mynth.songp=startp;
  mynth.song=v;
  mynth.songc=c;
  mynth.songstartp=startp;
  mynth.songloopp=loopp;
}
