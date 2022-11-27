#include "linuxglx_internal.h"

/* Callback.
 */
 
static void _cb_pcm(int16_t *v,int c,void *userdata) {
  //TODO synthesizer
  memset(v,0,c);
  linuxglx.aframec+=c/alsapcm_get_chanc(linuxglx.alsapcm);
}

/* Init.
 */
 
int linuxglx_audio_init() {
  fprintf(stderr,"%s...\n",__func__);

  struct alsapcm_delegate delegate={
    .pcm_out=_cb_pcm,
  };
  struct alsapcm_setup setup={
    //TODO Expose audio settings for command-line config.
    .rate=22050,
    .chanc=1,
    .device=0,
    .buffersize=0,
  };
  if (!(linuxglx.alsapcm=alsapcm_new(&delegate,&setup))) {
    fprintf(stderr,"%s:WARNING: Failed to create ALSA context. Proceeding without audio.\n",linuxglx.exename);
    return 0;
  }
  
  //TODO synthesizer

  return 0;
}

/* Event from client.
 */
 
void gamek_platform_audio_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  //TODO synthesizer
  fprintf(stderr,"%s:%d:%s: %02x %02x %02x %02x\n",__FILE__,__LINE__,__func__,chid,opcode,a,b);
}
