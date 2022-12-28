#include "mkdata_song.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Cleanup.
 */
 
void mkdata_song_cleanup(struct mkdata_song *song) {
  if (song->trackv) free(song->trackv);
  if (song->eventv) free(song->eventv);
}

/* Receive MThd.
 */
 
static int mkdata_song_decode_MThd(struct mkdata_song *song,const uint8_t *src,int srcc) {
  if (song->division) {
    fprintf(stderr,"%s: Multiple MThd\n",song->path);
    return -2;
  }
  if (srcc<6) {
    fprintf(stderr,"%s: Short MThd (%d<6)\n",song->path,srcc);
    return -2;
  }
  
  song->format=(src[0]<<8)|src[1];
  song->track_count=(src[2]<<8)|src[3];
  song->division=(src[4]<<8)|src[5];
  
  if (!song->division) {
    fprintf(stderr,"%s: Invalid MThd (division==0)\n",song->path);
    return -2;
  }
  if (song->division&0x8000) {
    fprintf(stderr,"%s: SMPTE timing not supported.\n",song->path);
    return -2;
  }
  
  return 0;
}

/* Receive MTrk.
 */
 
static int mkdata_song_decode_MTrk(struct mkdata_song *song,const void *src,int srcc) {
  if (song->trackc>=song->tracka) {
    int na=song->tracka+8;
    if (na>INT_MAX/sizeof(struct mkdata_track)) return -1;
    void *nv=realloc(song->trackv,sizeof(struct mkdata_track)*na);
    if (!nv) return -1;
    song->trackv=nv;
    song->tracka=na;
  }
  struct mkdata_track *track=song->trackv+song->trackc++;
  memset(track,0,sizeof(struct mkdata_track));
  track->v=src;
  track->c=srcc;
  track->trackid=song->trackc-1;
  track->delay=-1;
  return 0;
}

/* Receive one chunk.
 * Process MThd fully, store MTrk, and ignore anything else.
 * Returns length consumed, including header. Or zero to terminate.
 */
 
static int mkdata_song_decode_chunk(struct mkdata_song *song,const void *src,int srcc) {
  if (srcc<1) return 0;
  if (srcc<8) return -1;
  int err;
  const uint8_t *SRC=src;
  int len=(SRC[4]<<24)|(SRC[5]<<16)|(SRC[6]<<8)|SRC[7];
  if (len<0) return -1;
  if (8>srcc-len) return -1;
  
       if (!memcmp(src,"MThd",4)) { if ((err=mkdata_song_decode_MThd(song,SRC+8,len))<0) return err; }
  else if (!memcmp(src,"MTrk",4)) { if ((err=mkdata_song_decode_MTrk(song,SRC+8,len))<0) return err; }
  else ; // unknown, whatever, ignore it.
  
  return 8+len;
}

/* Read delay and update track state.
 */
 
static int mkdata_song_read_delay(struct mkdata_song *song,struct mkdata_track *track) {
  int err=sr_vlq_decode(&track->delay,track->v+track->p,track->c-track->p);
  if (err<1) return -1;
  track->p+=err;
  return 0;
}

/* Decode event.
 * The leading byte is provided separately as it may have come from Running Status.
 * In general, we don't read or write (song) or (track).
 */
 
static int mkdata_song_event_decode(
  struct mkdata_event *event,
  uint8_t lead,
  const uint8_t *src,int srcc,
  struct mkdata_song *song,
  struct mkdata_track *track
) {
  event->opcode=lead&0xf0;
  event->chid=lead&0x0f;
  #define A { if (srcc<1) return -1; event->a=src[0]; }
  #define AB { if (srcc<2) return -2; event->a=src[0]; event->b=src[1]; }
  switch (event->opcode) {
    case 0x80: AB return 2;
    case 0x90: AB if (!event->b) { event->opcode=0x80; event->b=0x40; } return 2;
    case 0xa0: AB return 2;
    case 0xb0: AB return 2;
    case 0xc0: A return 1;
    case 0xd0: A return 1;
    case 0xe0: AB return 2;
    case 0xf0: {
        event->opcode=lead;
        event->chid=0xff;
        switch (event->opcode) {
          case 0xff: {
              A
              int srcp,len;
              if ((srcp=sr_vlq_decode(&len,src+1,srcc-1))<1) return -1;
              srcp++;
              if (srcp>srcc-len) return -1;
              event->v=src+srcp;
              event->c=len;
              return srcp+len;
            }
          case 0xf0: case 0xf7: {
              int srcp,len;
              if ((srcp=sr_vlq_decode(&len,src,srcc))<1) return -1;
              if (srcp>srcc-len) return -1;
              event->v=src+srcp;
              event->c=len;
              return srcp+len;
            }
        }
      } break;
  }
  #undef A
  #undef AB
  return -1;
}

/* Grow event list.
 */
 
static int mkdata_song_eventv_require(struct mkdata_song *song,int addc) {
  if (addc<1) return 0;
  if (song->eventc>INT_MAX-addc) return -1;
  int na=song->eventc+addc;
  if (na<=song->eventa) return 0;
  if (na<INT_MAX-256) na=(na+256)&~255;
  if (na>INT_MAX/sizeof(struct mkdata_event)) return -1;
  void *nv=realloc(song->eventv,sizeof(struct mkdata_event)*na);
  if (!nv) return -1;
  song->eventv=nv;
  song->eventa=na;
  return 0;
}

/* Read one event, update track state, add event to list.
 * This does not include the leading delay.
 */
 
static int mkdata_song_read_event(struct mkdata_song *song,struct mkdata_track *track) {

  if (track->p>=track->c) return -1;
  uint8_t lead=track->v[track->p];
  if (lead&0x80) {
    track->status=lead;
    track->p++;
  } else if (!track->status) {
    return -1;
  } else {
    lead=track->status;
  }
  
  if (mkdata_song_eventv_require(song,1)<0) return -1;
  struct mkdata_event *event=song->eventv+song->eventc++;
  memset(event,0,sizeof(struct mkdata_event));
  event->trackid=track->trackid;
  event->time=song->time;
  event->duration=-1;
  
  int err=mkdata_song_event_decode(event,lead,track->v+track->p,track->c-track->p,song,track);
  if (err<0) return -1;
  track->p+=err;
  
  if ((event->opcode<0x80)||(event->opcode>=0xf0)) track->status=0;
  track->delay=-1;
  
  return 0;
}

/* Determine tempo by looking for any Meta Set Tempo event, or assume a default.
 * Issues a warning if the tempo changes, and retains the first one.
 * Returns tempo in ms/input tick.
 */
 
static double mkdata_song_calculate_tempo(struct mkdata_song *song) {
  int usperqnote=0;
  const struct mkdata_event *event=song->eventv;
  int i=song->eventc;
  for (;i-->0;event++) {
    if (event->opcode!=0xff) continue; // Meta
    if (event->a!=0x51) continue; // Set Tempo
    if (!event->v||(event->c!=3)) continue;
    const uint8_t *B=event->v;
    if (usperqnote) {
      fprintf(stderr,
        "%s:WARNING: Tempo change from 0x%06x to 0x%02x%02x%02x us/qnote. We will use the first one only.\n",
        song->path,usperqnote,B[0],B[1],B[2]
      );
      continue;
    }
    usperqnote=(B[0]<<16)|(B[1]<<8)|B[2];
  }
  if (!usperqnote) usperqnote=500000; // Default 120 bpm.
  double uspertick=(double)usperqnote/(double)song->division;
  return uspertick/1000.0;
}

/* End of event acquisition.
 * Confirm we're in a clean state, append extra events to make it so.
 * Consider the final delay, is it too long? Too short?
 */
 
static int mkdata_song_finalize_events(struct mkdata_song *song) {
  
  /* If we ended with a suspiciously long delay, trim it down and issue a warning.
   * Logic Pro X emits files like this and I have no idea why.
   */
  if (song->eventc>=2) {
    struct mkdata_event *ultimo=song->eventv+song->eventc-1;
    struct mkdata_event *penultimo=ultimo-1;
    int final_delay=ultimo->time-penultimo->time;
    int measurelen=song->division<<2;
    if ((final_delay>=measurelen)&&(ultimo->opcode>=0xf0)) {
      fprintf(stderr,"%s:WARNING: Trimming suspiciously long final delay of %d ticks.\n",song->path,final_delay);
      ultimo->time=penultimo->time;
    }
  }
  
  /* Convert all (time) from absolute ticks to absolute milliseconds.
   */
  double mspertick=mkdata_song_calculate_tempo(song);
  if (mspertick<=0.0) return -2;
  struct mkdata_event *event=song->eventv;
  int i=song->eventc;
  for (;i-->0;event++) {
    event->time=lround(event->time*mspertick);
  }
  
  /* Associate on and off events.
   * I'm going to do this with a static list, and that creates a limit on concurrent held notes.
   * Shouldn't be a big deal to make the list dynamic or to search again on each event, if the limit causes problems.
   */
  const int pendinga=128;
  int pendingc=0;
  struct mkdata_event *pendingv[pendinga];
  for (event=song->eventv,i=song->eventc;i-->0;event++) {
    if (event->opcode==0x90) {
      if (pendingc<pendinga) {
        pendingv[pendingc++]=event;
      } else {
        fprintf(stderr,
          "%s:WARNING: Exceeded concurrent note limit (%d). Rekajigger around %s:%d.\n",
          song->path,pendinga,__FILE__,__LINE__
        );
      }
    } else if (event->opcode==0x80) {
      struct mkdata_event *on=0;
      struct mkdata_event **q=pendingv+pendingc-1;
      int j=pendingc;
      for (;j-->0;q--) {
        // I think a strict reading of MIDI would say that On and Off don't need to be on the same track.
        // My gut says enforce a same-track rule anyway, since why would they ever be separate?
        // And I have seen multi-track MIDI files where each track uses the same channel unexpectedly.
        if ((*q)->trackid!=event->trackid) continue;
        if ((*q)->chid!=event->chid) continue;
        if ((*q)->a!=event->a) continue;
        on=*q;
        pendingc--;
        memmove(q,q+1,sizeof(void*)*(pendingc-j));
        break;
      }
      if (!on) {
        // Not sure this warrants a warning...
        fprintf(stderr,
          "%s:WARNING: Note Off 0x%02x, channel 0x%x, track %d/%d, Note On not found.\n",
          song->path,event->a,event->chid,event->trackid,song->trackc
        );
      } else {
        on->duration=event->duration=event->time-on->time;
      }
    }
  }
  if (pendingc) {
    fprintf(stderr,"%s:WARNING: %d Note On events were never Offed:\n",song->path,pendingc);
    struct mkdata_event **e=pendingv;
    for (i=pendingc;i-->0;e++) {
      fprintf(stderr,
        "  time=%d track=%d channel=%d noteid=%02x velocity=%02x\n",
        (*e)->time,(*e)->trackid,(*e)->chid,(*e)->a,(*e)->b
      );
    }
  }
  
  return 0;
}

/* Read all tracks as if we're playing back, and capture events.
 */
 
static int mkdata_song_decode_events(struct mkdata_song *song) {
  for (;;) {
  
    // Update all tracks. Consume events at current time and record the lowest track delay.
    int mindelay=INT_MAX,err;
    struct mkdata_track *track=song->trackv;
    int i=song->trackc;
    for (;i-->0;track++) {
      for (;;) {
        if (track->p>=track->c) break;
        if (track->delay<0) {
          if ((err=mkdata_song_read_delay(song,track))<0) return err;
          if (track->p>=track->c) break;
        }
        if (track->delay) break;
        if ((err=mkdata_song_read_event(song,track))<0) return err;
      }
      if (track->p>=track->c) continue;
      if (track->delay<mindelay) mindelay=track->delay;
    }
    
    // We didn't get a valid delay, so the song must be over.
    if (mindelay==INT_MAX) break;
    
    // Drop that minimum delay from all tracks and add it to our global clock.
    song->time+=mindelay;
    for (track=song->trackv,i=song->trackc;i-->0;track++) {
      if (track->p>=track->c) continue;
      if (track->delay<mindelay) return -1; // should not be possible
      track->delay-=mindelay;
    }
  }
  return mkdata_song_finalize_events(song);
}

/* Decode, main entry point.
 */

int mkdata_song_decode(struct mkdata_song *song,const void *src,int srcc,const char *path) {
  if (!song||(srcc<0)||(srcc&&!src)) return -1;
  if (!path) path="<unknown>";
  
  song->path=path;
  song->format=0;
  song->track_count=0;
  song->division=0;
  song->trackc=0;
  song->eventc=0;
  song->time=0;
  
  int srcp=0,err;
  while (srcp<srcc) {
    if ((err=mkdata_song_decode_chunk(song,(char*)src+srcp,srcc-srcp))<0) {
      if (err!=-2) fprintf(stderr,"%s:%d/%d: Failed to decode MIDI chunk.\n",song->path,srcp,srcc);
      return -2;
    }
    if (!err) break;
    srcp+=err;
  }
  
  if (!song->division) {
    fprintf(stderr,"%s: No MThd chunk.\n",path);
    return -2;
  }
  
  if ((err=mkdata_song_decode_events(song))<0) {
    if (err!=2) fprintf(stderr,"%s: Error reading events from MTrk chunks.\n",song->path);
    return -2;
  }
  
  return 0;
}

/* Encode to gamek song, in context, validated.
 */
 
static int mkdata_song_finish_inner(
  struct sr_encoder *dst,
  struct mkdata_song *song,
  int mspertick,int loopeventp
) {

  // 4-byte header.
  if (sr_encode_u8(dst,mspertick)<0) return -1;
  if (sr_encode_u8(dst,4)<0) return -1; // startp, ie length of this header.
  if (sr_encode_raw(dst,"\0\x04",2)<0) return -1; // loopp, will fill in later. Or 4 if we somehow fail to.
  
  fprintf(stderr,"reencoding song...\n");
  
  // Events.
  int pvtime=0;
  const struct mkdata_event *event=song->eventv;
  int i=song->eventc;
  for (;i-->0;event++) {
  
    // Delay.
    if (event->time>pvtime) {
      int delay=event->time-pvtime;
      while (delay>0xfff) {
        if (sr_encode_raw(dst,"\x8f\xff",2)<0) return -1;
        delay-=0xfff;
      }
      if (delay>0x7f) {
        if (sr_encode_intbe(dst,0x8000|delay,2)<0) return -1;
      } else if (delay>0) {
        if (sr_encode_u8(dst,delay)<0) return -1;
      }
      pvtime=event->time;
    }
    
    // Note Once.
    if ((event->opcode==0x90)&&(event->duration>=0)) {
      // 1001cccc NNNNNNNv vvDDDDDD One-shot note. (c) chid, (N) noteid, (v) velocity, (D) duration ticks.
      uint8_t tmp[]={0x90|event->chid,(event->a<<1)|(event->b>>6),((event->b<<2)&0xc0)|event->duration};
      if (sr_encode_raw(dst,tmp,sizeof(tmp))<0) return -1;
      continue;
    }
    
    // Note On.
    if (event->opcode==0x90) {
      // 1010cccc 0nnnnnnn 0vvvvvvv Note On. (c) chid, (n) noteid, (v) velocity.
      uint8_t tmp[]={0xa0|event->chid,event->a,event->b};
      if (sr_encode_raw(dst,tmp,sizeof(tmp))<0) return -1;
      continue;
    }
    
    // Note Off.
    if (event->opcode==0x80) {
      // 1011cccc 0nnnnnnn          Note Off.
      uint8_t tmp[]={0xb0|event->chid,event->a};
      if (sr_encode_raw(dst,tmp,sizeof(tmp))<0) return -1;
      continue;
    }
    
    // Control Change.
    if (event->opcode==0xb0) {
      // 1100cccc kkkkkkkk vvvvvvvv Control Change. (k,v) normally in 0..127, but we do allow the full 8 bits.
      uint8_t tmp[]={0xc0|event->chid,event->a,event->b};
      if (sr_encode_raw(dst,tmp,sizeof(tmp))<0) return -1;
      continue;
    }
    
    // Pitch Wheel.
    if (event->opcode==0xe0) {
      // 1101dddd vvvvvvvv          Pitch wheel. unsigned: 0x80 is default.
      uint8_t tmp[]={0xd0|event->chid,(event->a>>6)|(event->b<<1)};
      if (sr_encode_raw(dst,tmp,sizeof(tmp))<0) return -1;
      continue;
    }
  }
  
  // EOF
  if (sr_encode_u8(dst,0x00)<0) return -1;
  
  return 0;
}

/* Encode to gamek song, public wrapper.
 */
 
int mkdata_song_finish(
  void *dstpp,
  struct mkdata_song *song,
  int mspertick,int loopeventp
) {
  if (!dstpp||!song) return -1;
  if ((mspertick<1)||(mspertick>255)) return -1;
  if ((loopeventp<0)||(loopeventp>=song->eventc)) return -1;
  struct sr_encoder encoder={0};
  int err=mkdata_song_finish_inner(&encoder,song,mspertick,loopeventp);
  if (err<0) {
    sr_encoder_cleanup(&encoder);
    return err;
  }
  *(void**)dstpp=encoder.v;
  return encoder.c;
}

/* Filter events.
 */
 
void mkdata_song_filter_events(
  struct mkdata_song *song,
  int (*filter)(const struct mkdata_song *song,const struct mkdata_event *event,void *userdata),
  void *userdata
) {
  if (!song||!filter) return;
  int i=song->eventc;
  struct mkdata_event *event=song->eventv+i-1;
  for (;i-->0;event--) {
    if (filter(song,event,userdata)) continue;
    song->eventc--;
    memmove(event,event+1,sizeof(struct mkdata_event)*(song->eventc-i));
  }
}

/* Search events by time.
 */
 
static int mkdata_song_eventv_search_time(const struct mkdata_song *song,int time) {
  int lo=0,hi=song->eventc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct mkdata_event *event=song->eventv+ck;
         if (time<event->time) hi=ck;
    else if (time>event->time) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add event.
 */
 
struct mkdata_event *mkdata_song_add_event(struct mkdata_song *song,int p,int time) {
  if (!song) return 0;
  if (p>song->eventc) return 0;
  if (mkdata_song_eventv_require(song,1)<0) return 0;
  
  if (p<0) {
    if (time<0) {
      p=song->eventc;
      time=0;
      if (song->eventc) time=song->eventv[song->eventc-1].time;
    } else {
      p=mkdata_song_eventv_search_time(song,time);
      if (p<0) p=-p-1;
      while ((p<song->eventc)&&(time==song->eventv[p].time)) p++;
    }
  } else if (time<0) {
    if (p>0) time=song->eventv[p-1].time;
    else time=0;
  } else {
    if (p&&(time<song->eventv[p-1].time)) return 0;
    if ((p<song->eventc)&&(time>song->eventv[p].time)) return 0;
  }
  
  struct mkdata_event *event=song->eventv+p;
  memmove(event+1,event,sizeof(struct mkdata_event)*(song->eventc-p));
  song->eventc++;
  memset(event,0,sizeof(struct mkdata_event));
  event->trackid=0xff;
  event->chid=0xff;
  event->duration=-1;
  event->time=time;
  return event;
}

/* Finalize note events.
 */
 
int mkdata_song_finalize_note_events(struct mkdata_song *song) {
  if (!song) return -1;
  int i=0; for (;i<song->eventc;i++) {
    struct mkdata_event *event=song->eventv+i; // don't iterate this; we might reallocate during iteration
    if (event->opcode!=0x90) continue;
    if (event->duration<=0x3f) continue; // cool to keep it as Note Once.
    int offtime=event->time+event->duration;
    event->duration=-1; // signal to use On/Off instead of Once.
    struct mkdata_event on=*event; // may reallocate; must stop using (event) here.
    struct mkdata_event *off=mkdata_song_add_event(song,-1,offtime);
    if (!off) return -1;
    off->trackid=on.trackid;
    off->chid=on.chid;
    off->opcode=0x80;
    off->a=on.a;
    off->b=0x40;
  }
  return 0;
}
