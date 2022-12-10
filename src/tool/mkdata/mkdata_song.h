/* mkdata_song.h
 * Transitional data structure for converting MIDI to Gamek Song.
 */

#ifndef MKDATA_SONG_H
#define MKDATA_SONG_H

#include <stdint.h>

struct mkdata_song {
  const char *path; // WEAK

  // MThd:
  uint16_t format;
  uint16_t track_count;
  uint16_t division; // Can't be zero, serves as "MThd detected" flag.
  
  // MTrk:
  struct mkdata_track {
    const uint8_t *v;
    int c;
    uint8_t trackid; // index in this list, but let's be explicit about it
    // Further state only used during initial decode:
    uint8_t status;
    int delay; // Negative if not read yet.
    int p; // >=c if terminated
  } *trackv;
  int trackc,tracka;

  // Expanded out from MTrks:
  struct mkdata_event {
    uint8_t trackid; // index of MTrk in file
    uint8_t opcode;
    uint8_t chid; // 0xff if not channel-specific.
    uint8_t a;
    uint8_t b;
    // Meta (opcode==0xff) and Sysex (opcode==0xf0,0xf7) must populate (v,c).
    const void *v;
    int c;
    // Event time is initially in input ticks, then milliseconds, then output ticks.
    int time;
    // Duration is zero if actually zero or irrelevant, negative if unknown, or the positive duration in (time)'s unit.
    // Typically present for Note On and Note Off; negative only if we couldn't find the partner event.
    int duration;
  } *eventv;
  int eventc,eventa;
  
  // Read head time in ticks, during decode.
  int time;
};

/* Initialize to zeroes, and call this when finished, even if decode failed.
 */
void mkdata_song_cleanup(struct mkdata_song *song);

/* (src) and (path) are both borrowed, and must remain constant throughout the life of (song).
 */
int mkdata_song_decode(struct mkdata_song *song,const void *src,int srcc,const char *path);

/* Produces the gamek song output.
 * On success, caller must free (*dstpp).
 * You must provide the header loose:
 *   (mspertick) 1..255; and event time and duration in ticks.
 *   (loopeventp) index in (song->eventv) to loop to, ie after you've done all the sticky channel setup.
 */
int mkdata_song_finish(
  void *dstpp,
  struct mkdata_song *song,
  int mspertick,int loopeventp
);

/* Calls (filter) for each event. You must return 0 to drop it or 1 to keep it.
 * We do not necessarily call with events in order (in fact I intended to run it backward).
 */
void mkdata_song_filter_events(
  struct mkdata_song *song,
  int (*filter)(const struct mkdata_song *song,const struct mkdata_event *event,void *userdata),
  void *userdata
);

/* (p) or (time) can be negative to have us figure it out.
 * Both negative to append without advancing time.
 * Mind that (time) must be in whatever scale the song currently is in.
 */
struct mkdata_event *mkdata_song_add_event(struct mkdata_song *song,int p,int time);

/* First you must convert all Note On event->(time,duration) to output ticks, and remove Note Offs.
 * Then call this to insert new Note Offs where the duration exceeds what's encodable as Note Once.
 */
int mkdata_song_finalize_note_events(struct mkdata_song *song);

#endif
