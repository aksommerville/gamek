#include "linuxglx_internal.h"
#include <unistd.h>

/* File ready for reading.
 */
 
static int linuxglx_update_fd(int fd) {
  
  if (linuxglx.evdev) {
    int err=evdev_update_fd(linuxglx.evdev,fd);
    if (err<0) return -1;
    if (err) return 0;
  }
  
  if (linuxglx.ossmidi) {
    int err=ossmidi_update_fd(linuxglx.ossmidi,fd);
    if (err<0) return -1;
    if (err) return 0;
  }
  
  return 0;
}

/* Rebuild pollfdv.
 */
 
static int linuxglx_pollfdv_add(int fd,void *dummy) {
  if (linuxglx.pollfdc>=linuxglx.pollfda) {
    int na=linuxglx.pollfda+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(linuxglx.pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    linuxglx.pollfdv=nv;
    linuxglx.pollfda=na;
  }
  struct pollfd *pollfd=linuxglx.pollfdv+linuxglx.pollfdc++;
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  pollfd->revents=0;
  return 0;
}
 
static int linuxglx_pollfdv_populate() {
  linuxglx.pollfdc=0;
  if (linuxglx.evdev) {
    if (evdev_for_each_fd(linuxglx.evdev,linuxglx_pollfdv_add,0)<0) return -1;
  }
  if (linuxglx.ossmidi) {
    if (ossmidi_for_each_fd(linuxglx.ossmidi,linuxglx_pollfdv_add,0)<0) return -1;
  }
  return 0;
}

/* Poll input files if there are any, or sleep.
 */
 
static int linuxglx_poll_or_sleep(int tous) {
  if (linuxglx_pollfdv_populate()<0) {
    linuxglx.terminate++;
    return -1;
  }
  if (linuxglx.pollfdc) {
    int toms=(tous+999)/1000;
    int err=poll(linuxglx.pollfdv,linuxglx.pollfdc,toms);
    if (err>0) {
      const struct pollfd *pollfd=linuxglx.pollfdv;
      int i=linuxglx.pollfdc;
      for (;i-->0;pollfd++) {
        if (!pollfd->revents) continue;
        if (linuxglx_update_fd(pollfd->fd)<0) {
          linuxglx.terminate++;
          return -1;
        }
      }
    }
  } else {
    usleep(tous);
  }
  return 0;
}

/* Poll or sleep, and update whatever polls.
 */
 
int linuxglx_io_update() {
  int64_t now=linuxglx_now_us();
  if (now>=linuxglx.nexttime) { // Ready to update, don't delay.
    linuxglx.nexttime+=linuxglx.frametime;
    if (linuxglx.nexttime<=now) { // We've been stalled more than one frame. Resync clock.
      linuxglx.clockfaultc++;
      linuxglx.nexttime=now+linuxglx.frametime;
    }
  } else { // Delay until nexttime
    while (now<linuxglx.nexttime) {
      int64_t sleeptime=(linuxglx.nexttime-now);
      if (sleeptime>LINUXGLX_SLEEP_LIMIT_US) { // Improbable next time. Resync clock.
        linuxglx.clockfaultc++;
        linuxglx.nexttime=now;
        break;
      }
      if (linuxglx_poll_or_sleep(sleeptime)<0) return -1;
      now=linuxglx_now_us();
    }
    linuxglx.nexttime+=linuxglx.frametime;
  }
  return 0;
}
