#include "linux_internal.h"
#include "opt/fs/gamek_fs.h"
#include <unistd.h>
#include <fcntl.h>

/* Pop one codepoint or keycode off stdin.
 * If it's Unicode, it's in 1..0x10ffff.
 * Otherwise it's 0x40000000 | USB-HID usage.
 * Codepoint is zero if it's something we could measure but didn't bother comprehending. Ignore it.
 * Return length consumed.
 */
 
static int linux_stdin_next(int *codepoint,const uint8_t *src,int srcc) {
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
 
static int linux_update_stdin() {
  uint8_t buf[256];
  int bufc=read(STDIN_FILENO,buf,sizeof(buf));
  if (bufc<=0) {
    gamek_linux.use_stdin=0;
    return 0;
  }
  int bufp=0; while (bufp<bufc) {
    int codepoint=0;
    int err=linux_stdin_next(&codepoint,buf+bufp,bufc-bufp);
    if (err<=0) break;
    bufp+=err;
    switch (codepoint) {
      case 0x1b: gamek_linux.terminate++; break;
      //TODO We could get fancy here and send events out to inmgr. We'd have to make sure they don't get mapped as player inputs.
      // (We can't do player input from stdin because we don't get OFF events).
    }
  }
  return 0;
}

/* File ready for reading.
 */
 
static int linux_update_fd(int fd) {

  if (fd==STDIN_FILENO) return linux_update_stdin();
  
  if (gamek_linux.evdev) {
    int err=evdev_update_fd(gamek_linux.evdev,fd);
    if (err<0) return -1;
    if (err) return 0;
  }
  
  if (gamek_linux.ossmidi) {
    int err=ossmidi_update_fd(gamek_linux.ossmidi,fd);
    if (err<0) return -1;
    if (err) return 0;
  }
  
  return 0;
}

/* Rebuild pollfdv.
 */
 
static int linux_pollfdv_add(int fd,void *dummy) {
  if (gamek_linux.pollfdc>=gamek_linux.pollfda) {
    int na=gamek_linux.pollfda+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(gamek_linux.pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    gamek_linux.pollfdv=nv;
    gamek_linux.pollfda=na;
  }
  struct pollfd *pollfd=gamek_linux.pollfdv+gamek_linux.pollfdc++;
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  pollfd->revents=0;
  return 0;
}
 
static int linux_pollfdv_populate() {
  gamek_linux.pollfdc=0;
  if (gamek_linux.evdev) {
    if (evdev_for_each_fd(gamek_linux.evdev,linux_pollfdv_add,0)<0) return -1;
  }
  if (gamek_linux.ossmidi) {
    if (ossmidi_for_each_fd(gamek_linux.ossmidi,linux_pollfdv_add,0)<0) return -1;
  }
  if (gamek_linux.use_stdin) {
    if (linux_pollfdv_add(STDIN_FILENO,0)<0) return -1;
  }
  return 0;
}

/* Poll input files if there are any, or sleep.
 */
 
static int linux_poll_or_sleep(int tous) {
  if (linux_pollfdv_populate()<0) {
    gamek_linux.terminate++;
    return -1;
  }
  if (gamek_linux.pollfdc) {
    int toms=(tous+999)/1000;
    int err=poll(gamek_linux.pollfdv,gamek_linux.pollfdc,toms);
    if (err>0) {
      const struct pollfd *pollfd=gamek_linux.pollfdv;
      int i=gamek_linux.pollfdc;
      for (;i-->0;pollfd++) {
        if (!pollfd->revents) continue;
        if (linux_update_fd(pollfd->fd)<0) {
          gamek_linux.terminate++;
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
 
int linux_io_update() {
  int64_t now=linux_now_us();
  if (now>=gamek_linux.nexttime) { // Ready to update, don't delay.
    gamek_linux.nexttime+=gamek_linux.frametime;
    if (gamek_linux.nexttime<=now) { // We've been stalled more than one frame. Resync clock.
      gamek_linux.clockfaultc++;
      gamek_linux.nexttime=now+gamek_linux.frametime;
    }
  } else { // Delay until nexttime
    while (now<gamek_linux.nexttime) {
      int64_t sleeptime=(gamek_linux.nexttime-now);
      if (sleeptime>LINUX_SLEEP_LIMIT_US) { // Improbable next time. Resync clock.
        gamek_linux.clockfaultc++;
        gamek_linux.nexttime=now;
        break;
      }
      if (linux_poll_or_sleep(sleeptime)<0) return -1;
      now=linux_now_us();
    }
    gamek_linux.nexttime+=gamek_linux.frametime;
  }
  return 0;
}

/* Set the default sandbox path.
 */
 
static int linux_set_sandbox(const char *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  if (gamek_linux.fs_sandbox) free(gamek_linux.fs_sandbox);
  gamek_linux.fs_sandbox=nv;
  return 0;
}
 
static int linux_set_default_sandbox() {
  if (gamek_linux.fs_sandbox) return -1;
  
  // If exename has a slash, use its directory part.
  // ie prefer to put data adjacent to the executable.
  int slashp=-1,i=0;
  for (;gamek_linux.exename[i];i++) {
    if (gamek_linux.exename[i]=='/') slashp=i;
  }
  if (slashp>0) return linux_set_sandbox(gamek_linux.exename,slashp);
  
  // ~/.gamek/${CLIENT}
  if (gamek_client.title&&gamek_client.title[0]) {
    const char *home=getenv("HOME");
    if (home&&home[0]) {
      char tmp[1024];
      int tmpc=snprintf(tmp,sizeof(tmp),"%s/.gamek/%s",home,gamek_client.title);
      if ((tmpc>0)&&(tmpc<sizeof(tmp))) return linux_set_sandbox(tmp,tmpc);
    }
  }
  
  // Anything else? Working directory? Static location like /usr/local/share/gamek/${CLIENT}?
  
  return -1;
}

/* Combine and validate path in sandbox.
 * Fails if too long or if it escapes the sandbox.
 */
 
static int linux_get_sandbox_path(char *dst,int dsta,const char *src) {
  if (!gamek_linux.fs_sandbox) {
    if (linux_set_default_sandbox()<0) return -1;
    if (gamek_mkdirp(gamek_linux.fs_sandbox)<0) return -1;
  }
  if (!src) return -1;
  if (!dst||(dsta<1)) return -1;
  
  // Confirm there's no double-dots.
  // I'm going to do this cheap, and forbid even double-dots in the middle of a base name.
  const char *v=src; for (;*v;v++) {
    if ((v[0]=='.')&&(v[1]=='.')) return -1;
  }
  
  int dstc=snprintf(dst,dsta,"%s/%s",gamek_linux.fs_sandbox,src);
  if (dstc<1) return -1;
  if (dstc>=dsta) return -1;
  return dstc;
}

/* Read file, public hook.
 */
 
int32_t gamek_platform_file_read(void *dst,int32_t dsta,const char *clientpath,int32_t seek) {
  if (!dst||(dsta<1)) return 0;
  char path[1024];
  if (linux_get_sandbox_path(path,sizeof(path),clientpath)<0) return -1;
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  if (seek&&(lseek(fd,seek,SEEK_SET)!=seek)) {
    close(fd);
    return -1;
  }
  int dstc=read(fd,dst,dsta);
  close(fd);
  return dstc;
}

/* Write file, public hooks.
 */
 
int32_t gamek_platform_file_write_all(const char *clientpath,const void *src,int32_t srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  char path[1024];
  if (linux_get_sandbox_path(path,sizeof(path),clientpath)<0) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
  if (fd<0) return -1;
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return srcc;
}

int32_t gamek_platform_file_write_part(const char *clientpath,int32_t seek,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  char path[1024];
  if (linux_get_sandbox_path(path,sizeof(path),clientpath)<0) return -1;
  int fd=open(path,O_WRONLY|O_CREAT,0666);
  if (fd<0) return -1;
  
  if (seek<0) {
    if (lseek(fd,0,SEEK_END)<0) {
      close(fd);
      return -1;
    }
  } else {
    int fp=lseek(fd,seek,SEEK_SET);
    while (fp<seek) {
      const char *zeroes="\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
      int wrc=seek-fp;
      if (wrc>16) wrc=16;
      int err=write(fd,zeroes,wrc);
      if (err<=0) {
        close(fd);
        return -1;
      }
      fp+=wrc;
    }
  }
  
  int err=write(fd,src,srcc);
  close(fd);
  return err;
}
