#include "linux_internal.h"

/* Expand to stereo or more, from mono.
 * It's not unusual for synthesizers to be mono only, nor for PCM output to be stereo only.
 */
 
static void linux_audio_expand_stereo(int16_t *v,int framec) {
  const int16_t *src=v+framec;
  int16_t *dst=v+(framec<<1);
  while (framec-->0) {
    src-=1;
    dst-=2;
    dst[0]=dst[1]=*src;
  }
}

static void linux_audio_expand_multi(int16_t *v,int framec,int chanc) {
  const int16_t *src=v+framec;
  int16_t *dst=v+framec*chanc;
  while (framec-->0) {
    src-=1;
    int i=chanc; while (i-->0) {
      dst--;
      *dst=*src;
    }
  }
}

/* Callback.
 */
 
static void _cb_pcm(int16_t *v,int c,void *userdata) {
  int chanc=alsapcm_get_chanc(gamek_linux.alsapcm);

  #if GAMEK_USE_mynth
    if (chanc==1) {
      int i=c;
      while (i>0xffff) {
        mynth_update(v,0xffff);
        i-=0xffff;
        v+=0xffff;
      }
      if (i>0) mynth_update(v,i);
    } else if (chanc==2) {
      int framec=c>>1;
      while (framec>0xffff) {
        mynth_update(v,0xffff);
        linux_audio_expand_stereo(v,0xffff);
        framec-=0xffff;
        v+=0x1fffe;
      }
      if (framec>0) {
        mynth_update(v,framec);
        linux_audio_expand_stereo(v,framec);
      }
    } else {
      int framec=c/chanc;
      while (framec>0xffff) {
        mynth_update(v,0xffff);
        linux_audio_expand_multi(v,0xffff,chanc);
        framec-=0xffff;
        v+=0xffff*chanc;
      }
      if (framec>0) {
        mynth_update(v,framec);
        linux_audio_expand_multi(v,framec,chanc);
      }
    }
    
  #else
    memset(v,0,c);
  #endif

  gamek_linux.aframec+=c/chanc;
}

/* Init.
 */
 
int linux_audio_init() {

  struct alsapcm_delegate delegate={
    .pcm_out=_cb_pcm,
  };
  struct alsapcm_setup setup={
    .rate=gamek_linux.audio_rate,
    .chanc=gamek_linux.audio_chanc,
    .device=gamek_linux.audio_device,
    .buffersize=gamek_linux.audio_buffer_size,
  };
  if (!(gamek_linux.alsapcm=alsapcm_new(&delegate,&setup))) {
    fprintf(stderr,"%s:WARNING: Failed to create ALSA context. Proceeding without audio.\n",gamek_linux.exename);
    return 0;
  }
  
  fprintf(stderr,
    "%s: Initialized audio. rate=%d chanc=%d\n",
    gamek_linux.exename,
    alsapcm_get_rate(gamek_linux.alsapcm),
    alsapcm_get_chanc(gamek_linux.alsapcm)
  );
  
  #if GAMEK_USE_mynth
    mynth_init(alsapcm_get_rate(gamek_linux.alsapcm));
  #endif

  return 0;
}

/* Client-visible audio hooks.
 */
 
void gamek_platform_audio_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  //fprintf(stderr,"%s:%d:%s: %02x %02x %02x %02x\n",__FILE__,__LINE__,__func__,chid,opcode,a,b);
  #if GAMEK_USE_mynth
    if (alsapcm_lock(gamek_linux.alsapcm)<0) return;
    mynth_event(chid,opcode,a,b);
    alsapcm_unlock(gamek_linux.alsapcm);
  #endif
}

void gamek_platform_play_song(const void *v,uint16_t c) {
  #if GAMEK_USE_mynth
    if (alsapcm_lock(gamek_linux.alsapcm)<0) return;
    mynth_play_song(v,c);
    alsapcm_unlock(gamek_linux.alsapcm);
  #endif
}

void gamek_platform_set_audio_wave(uint8_t waveid,const int16_t *v,uint16_t c) {
  #if GAMEK_USE_mynth
    if (c==512) {
      if (alsapcm_lock(gamek_linux.alsapcm)<0) return;
      mynth_set_wave(waveid,v);
      alsapcm_unlock(gamek_linux.alsapcm);
    }
  #endif
}

void gamek_platform_audio_configure(const void *v,uint16_t c) {
  #if GAMEK_USE_mynth
    // Mynth doesn't have config like this.
  #endif
}
