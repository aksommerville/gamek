#include <stdio.h>
#include <math.h>
#include "mkdata_song.h"

/* Filter event.
 */
 
static int mkdata_song_from_midi_filter(
  const struct mkdata_song *song,
  const struct mkdata_event *event,
  void *userdata
) {
  
  // Note On events get a duration. No sense keeping the Offs (even if they have no partner).
  if (event->opcode==0x80) return 0;
  
  // Note On without a corresponding Off, go ahead and drop it.
  if ((event->opcode==0x90)&&(event->duration<0)) return 0;
  
  // We don't do aftertouch, so discard both Note Adjust and Channel Pressure.
  if (event->opcode==0xa0) return 0;
  if (event->opcode==0xd0) return 0;
  
  // Set Tempo has already been found, so no more interest in Meta or Sysex events.
  //TODO There might be some need for the text Meta events, like Instrument Name?
  if (event->opcode>=0xf0) return 0;
  
  return 1;
}

/* Choose the output tick length in ms, 1..255.
 * Can't fail.
 */
 
static int mkdata_song_from_midi_calculate_output_tempo(struct mkdata_song *song) {
  if (song->eventc<1) return 1;
  
  /* I'm going to take a pretty simplistic approach here.
   * Find the longest delay between events in ms, then select a tempo such that that delay fits in a short event (<0x80).
   * If the input is well-quantized, and suited to a specific set of harmonic delays, we won't notice.
   */
  int pvtime=0,longest=0;
  const struct mkdata_event *event=song->eventv;
  int i=song->eventc;
  for (;i-->0;event++) {
    int delayms=event->time-pvtime;
    if (delayms>0) {
      if (delayms>longest) longest=delayms;
      pvtime=event->time;
    }
  }
  
  int delay=(longest+126)/127;
  if (delay<1) return 1;
  if (delay>0x7f) return 0x7f;
  return delay;
}

/* Gamek song from MIDI, main entry point.
 */
 
int mkdata_song_from_midi(void *dstpp,const void *src,int srcc,const char *path) {
  struct mkdata_song song={0};
  int err;
  
  // Read MIDI file, split out events, convert event times to absolute ms.
  if ((err=mkdata_song_decode(&song,src,srcc,path))<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to decode MIDI file.\n",path);
    mkdata_song_cleanup(&song);
    return -2;
  }
  
  // Grab total time, then filter out unused events.
  // Total time should come from a Meta End of Track event, which is about to get filtered away.
  int totalms=0;
  if (song.eventc>0) totalms=song.eventv[song.eventc-1].time;
  mkdata_song_filter_events(&song,mkdata_song_from_midi_filter,0);
  
  // Append a new End of Track event, just one, to preserve the end time.
  struct mkdata_event *eos=mkdata_song_add_event(&song,-1,totalms);
  if (!eos) {
    mkdata_song_cleanup(&song);
    return -1;
  }
  
  /*TODO Determine instrument per channel.
   * I'd like to drive this mostly from a Program Change on the input.
   * Could use Meta Instrument Name too, that might be more convenient with MIDIs from Logic.
   * Add the appropriate Control Changes at time zero for each channel:
   *   0x07 Volume
   *   0x0c Attack time ms, minimum or no-velocity
   *   0x0d Attack level, ''
   *   0x0e Decay time ms, ''
   *   0x0f Sustain level, ''
   *   0x10 Release time 8ms, ''
   *   0x2c Attack time ms, maximum velocity
   *   0x2d Attack level, ''
   *   0x2e Decay time ms, ''
   *   0x2f Sustain level, ''
   *   0x30 Release time 8ms, ''
   *   0x40 Enable sustain
   *   0x46 Wave select, 0..7
   *   0x47 Wheel range, dimes.
   *   0x48 Warble range, dimes.
   *   0x49 Warble rate, hz.
   * Remove other Program Change and Control Changes.
   * Record in (loopeventp) the end of the new setup block.
   */
  int loopeventp=0;
   
  /* Determine and apply final timing.
   */
  int mspertick=mkdata_song_from_midi_calculate_output_tempo(&song);
  if (mspertick>1) {
    int half=mspertick>>1;
    struct mkdata_event *event=song.eventv;
    int i=song.eventc;
    for (;i-->0;event++) {
      event->time=(event->time+half)/mspertick;
      if (event->duration>0) event->duration=(event->duration+half)/mspertick;
    }
  }
  
  // Now that events are in output ticks, determine where we need to separate Note On and Note Off events.
  if (mkdata_song_finalize_note_events(&song)<0) {
    mkdata_song_cleanup(&song);
    return -1;
  }
  
  // Encode to gamek format.
  int dstc=mkdata_song_finish(dstpp,&song,mspertick,loopeventp);
  mkdata_song_cleanup(&song);
  if (dstc<0) {
    if (dstc!=-2) fprintf(stderr,"%s: Failed to reencode as gamek song.\n",path);
    return -2;
  }
  return dstc;
}
