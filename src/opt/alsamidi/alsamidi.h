/* alsamidi.h
 * Library-free alternative to libasound's Virtual RawMIDI API, same idea.
 * When you create an alsamidi context, a new MIDI Output device will appear on the bus.
 * Anybody connects to that and sends it stuff, the stuff will fall out your callback during updates.
 * Note that events do get digested by the kernel. You can't write arbitrary data to the device and expect it to pass through.
 */
 
#ifndef ALSAMIDI_H
#define ALSAMIDI_H

struct alsamidi;

struct alsamidi_delegate {
  void *userdata;
  
  /* Called during alsamidi_update() or alsamidi_read() if there is content incoming.
   * You will see each serial batch just once, then it's gone.
   * If the connection is lost, we send a farewell callback with (v,c)==(0,0).
   * (NB that's the receiver's connection, not presence of a sender on the other end -- you don't get to know that).
   * We always call with just one event at a time.
   */
  void (*cb)(const void *v,int c,void *userdata);
};

void alsamidi_del(struct alsamidi *alsamidi);

/* (path) can be null for the default "/dev/snd/seq".
 * (name) will be exposed on the bus. If null or empty, default "Virtual ALSA MIDI Output".
 */
struct alsamidi *alsamidi_new(
  const char *path,
  const char *name,
  const struct alsamidi_delegate *delegate
);

int alsamidi_update(struct alsamidi *alsamidi,int toms);

void *alsamidi_get_userdata(const struct alsamidi *alsamidi);
int alsamidi_get_fd(const struct alsamidi *alsamidi);

/* You can call this directly if the file descriptor polls.
 * (alsamidi) must be valid.
 * We don't check whether (fd) is owned by this alsamidi.
 */
int alsamidi_read(struct alsamidi *alsamidi,int fd);

#endif
