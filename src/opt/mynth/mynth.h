/* mynth.h
 * Minimal synthesizer.
 * This one is intended for Arduinos. Minimal memory and compute requirements.
 * We use global state, to avoid having to derefence some context object all the time.
 */
 
#ifndef MYNTH_H
#define MYNTH_H

#include <stdint.h>

/* Replace one entry of the global wave table.
 * Note that this can be done before init or after.
 * (waveid) is 0..7.
 * (v) must be the expected length, currently 512.
 * You don't need to provide a basic sine wave; we make one ourselves.
 */
void mynth_set_wave(uint8_t waveid,const int16_t *v);

/* (rate>0), in Hertz.
 * We recalculate the global rate table at init.
 * To keep memory usage down, there is only one copy of this table.
 * So round-off error will cause it to drift if you init more than once, be careful.
 */
void mynth_init(int32_t rate);

void mynth_update(int16_t *v,uint16_t c);
void mynth_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b);

/* We will read directly off (v) in real time. You must keep it constant.
 */
void mynth_play_song(const void *v,uint16_t c);

#endif
