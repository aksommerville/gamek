#include "alsamidi_internal.h"

/* Delete.
 */

void alsamidi_del(struct alsamidi *alsamidi) {
  if (!alsamidi) return;
  
  if (alsamidi->fd>=0) close(alsamidi->fd);
  if (alsamidi->name) free(alsamidi->name);
  
  free(alsamidi);
}

/* Open device.
 */
 
static int alsamidi_open(struct alsamidi *alsamidi,const char *path) {
  if ((alsamidi->fd=open(path,O_RDONLY))<0) return -1;
  
  // Not going to validate this, just confirming that we do have an ALSA Sequencer device.
  int pversion=0;
  if (ioctl(alsamidi->fd,SNDRV_SEQ_IOCTL_PVERSION,&pversion)<0) return -1;
  
  if (ioctl(alsamidi->fd,SNDRV_SEQ_IOCTL_CLIENT_ID,&alsamidi->client_id)<0) return -1;
  
  struct snd_seq_port_info port_info={
    .addr={
      .client=alsamidi->client_id,
      .port=1, // 0..255
    },
    .capability=
      SNDRV_SEQ_PORT_CAP_WRITE| // this messes with my head... Why "WRITE" if we intend to READ from it?
      SNDRV_SEQ_PORT_CAP_SYNC_WRITE|
      SNDRV_SEQ_PORT_CAP_SUBS_WRITE|
    0,
    .type=
      SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC|
      SNDRV_SEQ_PORT_TYPE_MIDI_GM|
      SNDRV_SEQ_PORT_TYPE_SOFTWARE|
      SNDRV_SEQ_PORT_TYPE_SYNTHESIZER|
      SNDRV_SEQ_PORT_TYPE_APPLICATION|
    0,
    .midi_channels=16,
    .midi_voices=128,
    .synth_voices=0,
  };
  strncpy(port_info.name,alsamidi->name,sizeof(port_info.name));
  if (ioctl(alsamidi->fd,SNDRV_SEQ_IOCTL_CREATE_PORT,&port_info)<0) return -1;
  
  return 0;
}

/* New.
 */

struct alsamidi *alsamidi_new(
  const char *path,
  const char *name,
  const struct alsamidi_delegate *delegate
) {
  if (!delegate||!delegate->cb) return 0;
  struct alsamidi *alsamidi=calloc(1,sizeof(struct alsamidi));
  if (!alsamidi) return 0;
  alsamidi->delegate=*delegate;
  alsamidi->fd=-1;
  
  if (!name||!name[0]) name="Virtual ALSA MIDI Output";
  if (!(alsamidi->name=strdup(name))) {
    alsamidi_del(alsamidi);
    return 0;
  }
  
  if (!path) path="/dev/snd/seq";
  if (alsamidi_open(alsamidi,path)<0) {
    alsamidi_del(alsamidi);
    return 0;
  }
  
  return alsamidi;
}

/* Process one ALSA Sequencer event and deliver to owner if warranted.
 * ALSA symbols are at /usr/include/sound/asequencer.h
 * These seem to cover all of MIDI, but with different values, and also a ton of other stuff.
 */
 
static void alsamidi_receive_event(
  struct alsamidi *alsamidi,
  const struct snd_seq_event *event
) {
  #define EV(...) { uint8_t tmp[]={__VA_ARGS__}; alsamidi->delegate.cb(tmp,sizeof(tmp),alsamidi->delegate.userdata); }
  #define D event->data
  switch (event->type) {
    case SNDRV_SEQ_EVENT_NOTEON: EV(0x90|D.note.channel,D.note.note,D.note.velocity) break;
    case SNDRV_SEQ_EVENT_NOTEOFF: EV(0x80|D.note.channel,D.note.note,D.note.velocity) break;
    case SNDRV_SEQ_EVENT_KEYPRESS: EV(0xa0|D.note.channel,D.note.note,D.note.velocity) break;
    case SNDRV_SEQ_EVENT_CONTROLLER: EV(0xb0|D.control.channel,D.control.param,D.control.value) break;
    case SNDRV_SEQ_EVENT_PGMCHANGE: EV(0xc0|D.control.channel,D.control.value) break;//TODO "value" or "param"?
    case SNDRV_SEQ_EVENT_CHANPRESS: EV(0xd0|D.control.channel,D.control.value) break;// ''
    case SNDRV_SEQ_EVENT_PITCHBEND: EV(0xe0|D.control.channel,D.control.value&0x7f,(D.control.value>>7)&0x7f) break;//TODO '', range
    // Deliberately not implementing Sysex.
    // Not bothering with most of the realtime events, because I don't feel they're important.
    case SNDRV_SEQ_EVENT_RESET: EV(0xff) break;
    // There are other administrative events that can tell us eg is another party connected.
    // Interesting but not interesting.
  }
  #undef EV
  #undef D
}

/* Read from file.
 */
 
int alsamidi_read(struct alsamidi *alsamidi,int fd) {
  /* Alas there does not appear to be serial MIDI streams at the kernel level; it's event-oriented down there.
   * So we need to parse the kernel's events and turn them back into MIDI.
   * (so the caller can turn that MIDI back into events....)
   */
  
  struct snd_seq_event eventv[32];
  int eventc=read(alsamidi->fd,eventv,sizeof(eventv));
  if (eventc<=0) {
    close(alsamidi->fd);
    alsamidi->fd=-1;
    alsamidi->delegate.cb(0,0,alsamidi->delegate.userdata);
    return 0;
  }
  eventc/=sizeof(eventv[0]);
  const struct snd_seq_event *event=eventv;
  for (;eventc-->0;event++) {
    alsamidi_receive_event(alsamidi,event);
  }
  return 0;
}

/* Update.
 */

int alsamidi_update(struct alsamidi *alsamidi,int toms) {
  if (!alsamidi) return 0;
  if (alsamidi->fd<0) {
    if (toms>0) usleep(toms/1000);
    return 0;
  }
  struct pollfd pollfd={.fd=alsamidi->fd,.events=POLLIN|POLLERR|POLLHUP};
  if (poll(&pollfd,1,toms)<=0) return 0;
  if (!pollfd.revents) return 0;
  return alsamidi_read(alsamidi,alsamidi->fd);
}

/* Trivial accessors.
 */

void *alsamidi_get_userdata(const struct alsamidi *alsamidi) {
  if (!alsamidi) return 0;
  return alsamidi->delegate.userdata;
}

int alsamidi_get_fd(const struct alsamidi *alsamidi) {
  if (!alsamidi) return -1;
  return alsamidi->fd;
}
