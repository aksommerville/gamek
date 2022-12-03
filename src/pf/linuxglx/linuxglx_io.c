#include "linuxglx_internal.h"
#include <unistd.h>

/* Pop one codepoint or keycode off stdin.
 * If it's Unicode, it's in 1..0x10ffff.
 * Otherwise it's 0x40000000 | USB-HID usage.
 * Codepoint is zero if it's something we could measure but didn't bother comprehending. Ignore it.
 * Return length consumed.
 */
 
static int linuxglx_stdin_next(int *codepoint,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  
  // Is it a VT escape?
  if ((src[0]==0x1b)&&(srcc>=2)&&(src[1]=='[')) {
    int srcp=2;
    while ((srcp<srcc)&&((src[srcp]<'a')||(src[srcp]>'z'))) srcp++;
    if (srcp>=srcc) { *codepoint=0; return srcc; }
    srcp++;
    //TODO We could check codes here, eg detect F1 ("\x1b[[A"). But I'm not currently using anything but ESC.
    // Return here if that changes.
    *codepoint=0;
    return srcp;
  }
  
  // Is it UTF-8?
  if (!(src[0]&0x80)) {
    *codepoint=src[0];
    return 1;
  }
  if (!(src[0]&0x40)) {
    if ((srcc>=2)&&!(src[0]&0x20)&&((src[1]&0xc0)==0x80)) {
      *codepoint=((src[0]&0x1f)<<6)|(src[1]&0x3f);
      return 2;
    } else if ((srcc>=3)&&!(src[0]&0x10)&&((src[1]&0xc0)==0x80)&&((src[2]&0xc0)==0x80)) {
      *codepoint=((src[0]&0x0f)<<12)|((src[1]&0x3f)<<6)|(src[2]&0x3f);
      return 3;
    } else if ((srcc>=4)&&!(src[0]&0x08)&&((src[1]&0xc0)==0x80)&&((src[2]&0xc0)==0x80)&&((src[3]&0xc0)==0x80)) {
      *codepoint=((src[0]&0x07)<<18)|((src[1]&0x3f)<<12)|((src[2]&0x3f)<<6)|(src[3]&0x3f);
      return 4;
    }
  }
  
  // Assume 8859-1.
  *codepoint=src[0];
  return 1;
}

/* Read stdin and fire actions.
 */
 
static int linuxglx_update_stdin() {
  uint8_t buf[256];
  int bufc=read(STDIN_FILENO,buf,sizeof(buf));
  if (bufc<=0) {
    linuxglx.use_stdin=0;
    return 0;
  }
  int bufp=0; while (bufp<bufc) {
    int codepoint=0;
    int err=linuxglx_stdin_next(&codepoint,buf+bufp,bufc-bufp);
    if (err<=0) break;
    bufp+=err;
    switch (codepoint) {
      case 0x1b: linuxglx.terminate++; break;
      //TODO We could get fancy here and send events out to inmgr. We'd have to make sure they don't get mapped as player inputs.
      // (We can't do player input from stdin because we don't get OFF events).
    }
  }
  return 0;
}

/* File ready for reading.
 */
 
static int linuxglx_update_fd(int fd) {

  if (fd==STDIN_FILENO) return linuxglx_update_stdin();
  
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
  if (linuxglx.use_stdin) {
    if (linuxglx_pollfdv_add(STDIN_FILENO,0)<0) return -1;
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
