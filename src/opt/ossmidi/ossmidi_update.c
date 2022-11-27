#include "ossmidi_internal.h"

/* Check one newly-discovered file under /dev
 */
 
static int ossmidi_check_file(struct ossmidi *ossmidi,const char *base,int basec) {

  // First, is it named like a MIDI device file?
  if ((basec<5)||memcmp(base,"midi",4)) return 0;
  int devid=0,basep=4;
  for (;basep<basec;basep++) {
    if (base[basep]<'0') return 0;
    if (base[basep]>'9') return 0;
    devid*=10;
    devid+=base[basep]-'0';
    if (devid>9999) return 0;
  }
  
  // Do we already have it open?
  if (ossmidi_index_by_devid(ossmidi,devid)>=0) return 0;
  
  // Make room for it.
  if (ossmidi->devicec>=ossmidi->devicea) {
    int na=ossmidi->devicea+8;
    if (na>INT_MAX/sizeof(struct ossmidi_device)) return -1;
    void *nv=realloc(ossmidi->devicev,sizeof(struct ossmidi_device)*na);
    if (!nv) return -1;
    ossmidi->devicev=nv;
    ossmidi->devicea=na;
  }
  
  // Open the file. No worries if we can't, just ignore it.
  char path[64];
  int pathc=snprintf(path,sizeof(path),"/dev/%.*s",basec,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  // Store it and tell the user.
  struct ossmidi_device *device=ossmidi->devicev+ossmidi->devicec++;
  device->devid=devid;
  device->fd=fd;
  if (ossmidi->delegate.connect) {
    ossmidi->delegate.connect(devid,fd,ossmidi->delegate.userdata);
  }
  ossmidi->delegate.events(devid,"\xf0\xf7",2,ossmidi->delegate.userdata);

  return 0;
}

/* Scan for new devices.
 */

int ossmidi_rescan_now(struct ossmidi *ossmidi) {
  if (!ossmidi) return -1;
  ossmidi->rescan=0;
  DIR *dir=opendir("/dev");
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if (ossmidi_check_file(ossmidi,base,basec)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Read inotify and process anything we find.
 */
 
static int ossmidi_update_inotify(struct ossmidi *ossmidi) {
  char buf[1024];
  int bufc=read(ossmidi->infd,buf,sizeof(buf));
  if (bufc<=0) {
    if (ossmidi->delegate.lost_inotify) {
      ossmidi->delegate.lost_inotify(ossmidi->delegate.userdata);
    }
    close(ossmidi->infd);
    ossmidi->infd=-1;
    return 1;
  }
  int bufp=0;
  while (1) {
    const struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc) break;
    const char *base=event->name;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    bufp+=event->len;
    if (ossmidi_check_file(ossmidi,base,basec)<0) return -1;
  }
  return 1;
}

/* Read and report from one device file.
 */
 
static int ossmidi_update_device(struct ossmidi *ossmidi,int fd) {
  int devp=ossmidi_index_by_fd(ossmidi,fd);
  if (devp<0) return 0;
  int devid=ossmidi->devicev[devp].devid;
  char buf[256];
  int bufc=read(fd,buf,sizeof(buf));
  if (bufc<=0) {
    ossmidi->delegate.events(devid,0,0,ossmidi->delegate.userdata);
    if (ossmidi->delegate.disconnect) {
      ossmidi->delegate.disconnect(devid,fd,ossmidi->delegate.userdata);
    }
    ossmidi->devicec--;
    memmove(ossmidi->devicev+devp,ossmidi->devicev+devp+1,sizeof(struct ossmidi_device)*(ossmidi->devicec-devp));
    return 1;
  }
  // Ensure we do not accidentally send the Hello event.
  if ((bufc==2)&&!memcmp(buf,"\xf0\xf7",2)) return 0;
  ossmidi->delegate.events(devid,buf,bufc,ossmidi->delegate.userdata);
  return 1;
}

/* Update one file.
 */
 
int ossmidi_update_fd(struct ossmidi *ossmidi,int fd) {
  if (!ossmidi) return -1;
  if (fd<0) return -1;
  if (fd==ossmidi->infd) return ossmidi_update_inotify(ossmidi);
  return ossmidi_update_device(ossmidi,fd);
}

/* Reallocate and populate pollfdv.
 */
 
static int ossmidi_rebuild_pollfdv(struct ossmidi *ossmidi) {
  int na=ossmidi->devicec;
  if (ossmidi->infd>=0) na++;
  if (na>ossmidi->pollfda) {
    if (na<INT_MAX-16) na=(na+16)&~15;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(ossmidi->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    ossmidi->pollfdv=nv;
    ossmidi->pollfda=na;
  }
  ossmidi->pollfdc=0;
  if (ossmidi->infd>=0) {
    struct pollfd *pollfd=ossmidi->pollfdv+ossmidi->pollfdc++;
    pollfd->fd=ossmidi->infd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
    pollfd->revents=0;
  }
  const struct ossmidi_device *device=ossmidi->devicev;
  int i=ossmidi->devicec;
  for (;i-->0;device++) {
    struct pollfd *pollfd=ossmidi->pollfdv+ossmidi->pollfdc++;
    pollfd->fd=device->fd;
    pollfd->events=POLLIN|POLLERR|POLLHUP;
    pollfd->revents=0;
  }
  return 0;
}

/* Update, top level.
 */

int ossmidi_update(struct ossmidi *ossmidi,int toms) {
  if (!ossmidi) return -1;
  if (ossmidi->rescan) {
    if (ossmidi_rescan_now(ossmidi)<0) return -1;
  }
  if (ossmidi_rebuild_pollfdv(ossmidi)<0) return -1;
  if (ossmidi->pollfdc<1) {
    if (toms>0) usleep(toms*1000);
    return 0;
  }
  if (poll(ossmidi->pollfdv,ossmidi->pollfdc,toms)<=0) return 0;
  const struct pollfd *pollfd=ossmidi->pollfdv;
  int i=ossmidi->pollfdc;
  for (;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (ossmidi_update_fd(ossmidi,pollfd->fd)<0) return -1;
  }
  return 1;
}
