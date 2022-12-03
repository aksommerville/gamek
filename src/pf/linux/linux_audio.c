#include "linux_internal.h"

/* Callback.
 */
 
static void _cb_pcm(int16_t *v,int c,void *userdata) {
  //TODO synthesizer
  memset(v,0,c);
  gamek_linux.aframec+=c/alsapcm_get_chanc(gamek_linux.alsapcm);
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
  
  //TODO synthesizer

  return 0;
}

/* Event from client.
 */
 
void gamek_platform_audio_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  //TODO synthesizer
  fprintf(stderr,"%s:%d:%s: %02x %02x %02x %02x\n",__FILE__,__LINE__,__func__,chid,opcode,a,b);
}
