#include "fiddle_internal.h"
#include <unistd.h>
#include <sys/poll.h>

/* Watch directories and files, inotify only.
 */
#if FIDDLE_USE_INOTIFY

static int fiddle_add_watchdir(const char *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  int i=fiddle.watchdirc;
  struct fiddle_watchdir *watchdir=fiddle.watchdirv;
  for (;i-->0;watchdir++) {
    if (watchdir->dirc!=srcc) continue;
    if (memcmp(src,watchdir->dir,watchdir->dirc)) continue;
    return watchdir->wd;
  }
  if (fiddle.watchdirc>=fiddle.watchdira) {
    int na=fiddle.watchdira+8;
    if (na>INT_MAX/sizeof(struct fiddle_watchdir)) return -1;
    void *nv=realloc(fiddle.watchdirv,sizeof(struct fiddle_watchdir)*na);
    if (!nv) return -1;
    fiddle.watchdirv=nv;
    fiddle.watchdira=na;
  }
  watchdir=fiddle.watchdirv+fiddle.watchdirc++;
  memset(watchdir,0,sizeof(struct fiddle_watchdir));
  if (!(watchdir->dir=malloc(srcc+1))) return -1;
  memcpy(watchdir->dir,src,srcc);
  watchdir->dir[srcc]=0;
  watchdir->dirc=srcc;
  if ((watchdir->wd=inotify_add_watch(fiddle.infd,watchdir->dir,IN_CREATE|IN_MOVED_TO|IN_ATTRIB))<0) return -1;
  return watchdir->wd;
}

static int fiddle_add_watchfile(int waveid,int wd,const char *base) {
  if (!base||!base[0]) return -1;
  int basec=0;
  while (base[basec]) basec++;
  // If we already have this waveid, replace it.
  struct fiddle_watchfile *watchfile=fiddle.watchfilev;
  int i=fiddle.watchfilec;
  for (;i-->0;watchfile++) {
    if (waveid==watchfile->waveid) {
      if (watchfile->base) free(watchfile->base);
      if (!(watchfile->base=malloc(basec+1))) return -1;
      memcpy(watchfile->base,base,basec+1);
      watchfile->basec=basec;
      watchfile->wd=wd;
      return 0;
    }
  }
  // Otherwise, add it.
  if (fiddle.watchfilec>=fiddle.watchfilea) {
    int na=fiddle.watchfilea+8;
    if (na>INT_MAX/sizeof(struct fiddle_watchfile)) return -1;
    void *nv=realloc(fiddle.watchfilev,sizeof(struct fiddle_watchfile)*na);
    if (!nv) return -1;
    fiddle.watchfilev=nv;
    fiddle.watchfilea=na;
  }
  watchfile=fiddle.watchfilev+fiddle.watchfilec++;
  memset(watchfile,0,sizeof(struct fiddle_watchfile));
  if (!(watchfile->base=malloc(basec+1))) return -1;
  memcpy(watchfile->base,base,basec+1);
  watchfile->basec=basec;
  watchfile->wd=wd;
  watchfile->waveid=waveid;
  return 0;
}

#endif

/* Add a source for monitoring.
 */
 
static int fiddle_add_wave_source(int waveid,const char *path) {
  #if FIDDLE_USE_INOTIFY
    if (fiddle.infd>=0) {
      int sepp=gamek_path_find_separator(path,-1);
      if (sepp<=0) {
        int wd=fiddle_add_watchdir(".",1);
        if (wd<0) return -1;
        if (fiddle_add_watchfile(waveid,wd,path)<0) return -1;
      } else {
        int wd=fiddle_add_watchdir(path,sepp);
        if (wd<0) return -1;
        if (fiddle_add_watchfile(waveid,wd,path+sepp+1)<0) return -1;
      }
    }
  #endif
  return 0;
}

/* Load and begin monitoring wave.
 */
 
int fiddle_set_wave(int waveid,const char *path,int pathc) {
  if ((waveid<0)||(waveid>7)) return 0;
  char zpath[1024];
  if ((pathc<1)||(pathc>=sizeof(zpath))) return -1;
  memcpy(zpath,path,pathc);
  zpath[pathc]=0;
  
  // Read text. File must exist, etc, in order to start using it.
  char *text=0;
  int textc=gamek_file_read(&text,zpath);
  if (textc<0) {
    fprintf(stderr,"%s: Failed to read wave file.\n",zpath);
    return -2;
  }
  
  // All wave files must compile initially, otherwise we emit an error and abort.
  int16_t *wave=0;
  int wavec=mkdata_wavebin_from_wave(&wave,text,textc,zpath);
  free(text);
  if (wavec!=FIDDLE_WAVE_SIZE*sizeof(int16_t)) {
    if (wavec==-2) ;
    else if (wavec<0) {
      fprintf(stderr,"%s: Unspecified error compiling wave.\n",zpath);
    } else {
      fprintf(stderr,"%s: Unexpected length %d for compiled wave, expected 512*2==1024.\n",zpath,wavec);
    }
    if (wave) free(wave);
    return -2;
  }
  
  int16_t *dst=fiddle.waves+FIDDLE_WAVE_SIZE*waveid;
  memcpy(dst,wave,FIDDLE_WAVE_SIZE*sizeof(int16_t));
  free(wave);
  
  if (fiddle_lock()<0) return -1;
  
  #if GAMEK_USE_mynth
    mynth_set_wave(waveid,dst);
  #endif
  
  fiddle_unlock();
  
  // Register file for monitoring.
  #if FIDDLE_USE_INOTIFY
    if (fiddle_add_wave_source(waveid,path)<0) return -1;
  #endif
  
  return 0;
}

/* Reload one file fingered by inotify.
 * There can and will be files fingered that we're not tracking; quietly ignore them.
 */
#if FIDDLE_USE_INOTIFY

static int fiddle_waves_reload(int wd,const char *base,int basec) {
  struct fiddle_watchfile *watchfile=fiddle.watchfilev;
  int i=fiddle.watchfilec;
  for (;i-->0;watchfile++) {
    if (watchfile->wd!=wd) continue;
    if (watchfile->basec!=basec) continue;
    if (memcmp(base,watchfile->base,basec)) continue;
    
    struct fiddle_watchdir *watchdir=0,*q=fiddle.watchdirv;
    int di=fiddle.watchdirc;
    for (;di-->0;q++) {
      if (q->wd==wd) {
        watchdir=q;
        break;
      }
    }
    if (!watchdir) return 0;
    
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%.*s/%.*s",watchdir->dirc,watchdir->dir,basec,base);
    if ((pathc<1)||(pathc>=sizeof(path))) return 0;
    
    char *text=0;
    int textc=gamek_file_read(&text,path);
    if (textc<0) return 0;
  
    int16_t *wave=0;
    int wavec=mkdata_wavebin_from_wave(&wave,text,textc,path);
    free(text);
    if (wavec!=FIDDLE_WAVE_SIZE*sizeof(int16_t)) {
      fprintf(stderr,"%s:ERROR: Failed to compile wave %d. Keeping what we had before.\n",path,watchfile->waveid);
      if (wave) free(wave);
      fiddle_unleash_rage();
      return 0;
    }
  
    int16_t *dst=fiddle.waves+FIDDLE_WAVE_SIZE*watchfile->waveid;
    memcpy(dst,wave,FIDDLE_WAVE_SIZE*sizeof(int16_t));
    free(wave);
  
    if (fiddle_lock()<0) return -1;
  
    #if GAMEK_USE_mynth
      mynth_set_wave(watchfile->waveid,dst);
    #endif
  
    fiddle_unlock();
    
    fprintf(stderr,"%s: Reloaded wave %d\n",path,watchfile->waveid);
    
    return 0;
  }
  return 0;
}

#endif

/* Monitor changes to waves.
 */
 
int fiddle_waves_update() {
  #if FIDDLE_USE_INOTIFY
    if (fiddle.infd>=0) {
      struct pollfd pollfd={.fd=fiddle.infd,.events=POLLIN|POLLERR|POLLHUP};
      if (poll(&pollfd,1,0)<=0) return 0;
      char buf[1024];
      int bufc=read(fiddle.infd,buf,sizeof(buf));
      if (bufc<=0) {
        fprintf(stderr,"fiddle:WARNING: Lost inotify connection. Changes to wave sources will not be noticed anymore.\n");
        close(fiddle.infd);
        fiddle.infd=-1;
        return 0;
      }
      int bufp=0;
      for (;;) {
        if (bufp>bufc-sizeof(struct inotify_event)) break;
        const struct inotify_event *event=(struct inotify_event*)(buf+bufp);
        bufp+=sizeof(struct inotify_event);
        bufp+=event->len;
        if (bufp>bufc) break;
        const char *base=event->name;
        int basec=0;
        while ((basec<event->len)&&base[basec]) basec++;
        if (fiddle_waves_reload(event->wd,base,basec)<0) return -1;
      }
    }
  #endif
  return 0;
}

/* Initialize waves monitor.
 */
 
int fiddle_waves_init() {
  #if FIDDLE_USE_INOTIFY
    if ((fiddle.infd=inotify_init())<0) return -1;
  #endif
  //TODO Alternative to inotify for MacOS and Windows.
  return 0;
}
