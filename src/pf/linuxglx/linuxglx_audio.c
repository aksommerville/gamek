#include "linuxglx_internal.h"

/* Callback.
 */
 
static void _cb_pcm(int16_t *v,int c,void *userdata) {
  //TODO Consider removing the generate_audio hook. Expose a more event-oriented audio API to the app layer, not a PCM one.
  if (!gamek_client.generate_audio) {
    memset(v,0,c<<1);
    return;
  }
  while (c>0xffff) {
    gamek_client.generate_audio(v,0xffff);
    v+=0xffff;
    c-=0xffff;
  }
  if (c>0) {
    gamek_client.generate_audio(v,c);
  }
}

/* Init.
 */
 
int linuxglx_audio_init() {
  fprintf(stderr,"%s...\n",__func__);

  if (!gamek_client.generate_audio) {
    return 0;
  }

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

  return 0;
}
