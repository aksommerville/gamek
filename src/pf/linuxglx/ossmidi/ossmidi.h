/* ossmidi.h
 * Access to individual MIDI input devices via OSS.
 * That API is emulated by ALSA, so it remains available in modern Linux.
 * (and to my taste, the OSS API is preferable to ALSA's own).
 */
 
#ifndef OSSMIDI_H
#define OSSMIDI_H

struct ossmidi;

/* Typical consumers should only implement (events).
 * We send Hello and Farewell as events, in addition to (connect) and (disconnect):
 *   - Hello: [0xf0,0xf7]
 *   - Farewell: []
 * Implement the other three callbacks if you want to use an external poller,
 * and use ossmidi_update_fd() instead of ossmidi_update().
 */
struct ossmidi_delegate {
  void *userdata;
  void (*events)(int devid,const void *v,int c,void *userdata);
  void (*connect)(int devid,int fd,void *userdata);
  void (*disconnect)(int devid,int fd,void *userdata);
  void (*lost_inotify)(void *userdata);
};

void ossmidi_del(struct ossmidi *ossmidi);

struct ossmidi *ossmidi_new(
  const struct ossmidi_delegate *delegate
);

void *ossmidi_get_userdata(const struct ossmidi *ossmidi);
int ossmidi_get_inotify_fd(const struct ossmidi *ossmidi);

// Request to scan for new connections at the next full update, or immediately.
void ossmidi_rescan(struct ossmidi *ossmidi);
int ossmidi_rescan_now(struct ossmidi *ossmidi);

/* Callbacks fire only during update. (and ossmidi_rescan_now()).
 * Use regular update and we will poll().
 * Or you can poll externally, and call ossmidi_update_fd() with anything readable.
 */
int ossmidi_update(struct ossmidi *ossmidi,int toms);
int ossmidi_update_fd(struct ossmidi *ossmidi,int fd);

int ossmidi_count_devices(const struct ossmidi *ossmidi);
int ossmidi_index_by_devid(const struct ossmidi *ossmidi,int devid);
int ossmidi_index_by_fd(const struct ossmidi *ossmidi,int fd);
int ossmidi_devid_by_index(const struct ossmidi *ossmidi,int p);
int ossmidi_fd_by_index(const struct ossmidi *ossmidi,int p);

int ossmidi_for_each_fd(
  const struct ossmidi *ossmidi,
  int (*cb)(int fd,void *userdata),
  void *userdata
);

#endif
