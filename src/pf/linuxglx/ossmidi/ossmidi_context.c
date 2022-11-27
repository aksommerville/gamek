#include "ossmidi_internal.h"

/* Delete.
 */

static void ossmidi_device_cleanup(struct ossmidi_device *device) {
  if (device->fd>=0) close(device->fd);
}

void ossmidi_del(struct ossmidi *ossmidi) {
  if (!ossmidi) return;
  if (ossmidi->infd>=0) close(ossmidi->infd);
  if (ossmidi->devicev) {
    while (ossmidi->devicec-->0) {
      ossmidi_device_cleanup(ossmidi->devicev+ossmidi->devicec);
    }
    free(ossmidi->devicev);
  }
  if (ossmidi->pollfdv) free(ossmidi->pollfdv);
  free(ossmidi);
}

/* New.
 */

struct ossmidi *ossmidi_new(
  const struct ossmidi_delegate *delegate
) {
  if (!delegate||!delegate->events) return 0;
  struct ossmidi *ossmidi=calloc(1,sizeof(struct ossmidi));
  if (!ossmidi) return 0;
  
  ossmidi->infd=-1;
  ossmidi->delegate=*delegate;
  ossmidi->rescan=1;
  
  if (
    ((ossmidi->infd=inotify_init())<0)||
    (inotify_add_watch(ossmidi->infd,"/dev",IN_ATTRIB|IN_CREATE|IN_MOVED_TO)<0)
  ) {
    ossmidi_del(ossmidi);
    return 0;
  }
  
  return ossmidi;
}

/* Trivial accessors.
 */

void *ossmidi_get_userdata(const struct ossmidi *ossmidi) {
  if (!ossmidi) return 0;
  return ossmidi->delegate.userdata;
}

int ossmidi_get_inotify_fd(const struct ossmidi *ossmidi) {
  if (!ossmidi) return -1;
  return ossmidi->infd;
}

void ossmidi_rescan(struct ossmidi *ossmidi) {
  if (!ossmidi) return;
  ossmidi->rescan=1;
}

/* Public access to device list.
 */
 
int ossmidi_count_devices(const struct ossmidi *ossmidi) {
  if (!ossmidi) return 0;
  return ossmidi->devicec;
}

int ossmidi_index_by_devid(const struct ossmidi *ossmidi,int devid) {
  if (!ossmidi) return -1;
  const struct ossmidi_device *device=ossmidi->devicev;
  int p=0;
  for (;p<ossmidi->devicec;p++,device++) {
    if (device->devid==devid) return p;
  }
  return -1;
}

int ossmidi_index_by_fd(const struct ossmidi *ossmidi,int fd) {
  if (!ossmidi) return -1;
  const struct ossmidi_device *device=ossmidi->devicev;
  int p=0;
  for (;p<ossmidi->devicec;p++,device++) {
    if (device->fd==fd) return p;
  }
  return -1;
}

int ossmidi_devid_by_index(const struct ossmidi *ossmidi,int p) {
  if (!ossmidi) return -1;
  if (p<0) return -1;
  if (p>=ossmidi->devicec) return -1;
  return ossmidi->devicev[p].devid;
}

int ossmidi_fd_by_index(const struct ossmidi *ossmidi,int p) {
  if (!ossmidi) return -1;
  if (p<0) return -1;
  if (p>=ossmidi->devicec) return -1;
  return ossmidi->devicev[p].fd;
}

/* Iterate files.
 */
 
int ossmidi_for_each_fd(
  const struct ossmidi *ossmidi,
  int (*cb)(int fd,void *userdata),
  void *userdata
) {
  if (!ossmidi||!cb) return 0;
  int err;
  if (ossmidi->infd>=0) {
    if (err=cb(ossmidi->infd,userdata)) return err;
  }
  const struct ossmidi_device *device=ossmidi->devicev;
  int i=ossmidi->devicec;
  for (;i-->0;device++) {
    if (err=cb(device->fd,userdata)) return err;
  }
  return 0;
}
