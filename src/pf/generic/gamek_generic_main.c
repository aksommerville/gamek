#include "gamek_generic_internal.h"
#include <signal.h>

struct gamek_generic gamek_generic={0};

/* Platform details.
 */
 
const struct gamek_platform_details gamek_platform_details={
  .name="generic",
  .capabilities=
    GAMEK_PLATFORM_CAPABILITY_TERMINATE|
    GAMEK_PLATFORM_CAPABILITY_MULTIPLAYER|
    GAMEK_PLATFORM_CAPABILITY_STORAGE|
    GAMEK_PLATFORM_CAPABILITY_NETWORK|
    GAMEK_PLATFORM_CAPABILITY_AUDIO|
  0,
};

/* Signal handler.
 */
 
static void _generic_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(gamek_generic.terminate)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Required platform hooks.
 */
 
void gamek_platform_terminate(uint8_t status) {
  gamek_generic.terminate++;
}

void gamek_platform_audio_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b) {
  // We lied about supporting audio. But could modify this hook if there's a reason to monitor sound effects coming from the game.
}

void gamek_platform_play_song(const void *v,uint16_t c) {}
void gamek_platform_set_audio_wave(uint8_t waveid,const int16_t *v,uint16_t c) {}
void gamek_platform_audio_configure(const void *v,uint16_t c) {}

int32_t gamek_platform_file_read(void *dst,int32_t dsta,const char *path,int32_t seek) {
  return 0;
}

int32_t gamek_platform_file_write_all(const char *path,const void *src,int32_t srcc) {
  return -1;
}

int32_t gamek_platform_file_write_part(const char *path,int32_t seek,const void *src,int srcc) {
  return -1;
}

const struct gamek_image *gamek_platform_get_sample_framebuffer() {
  return &gamek_generic.fb;
}

/* Main.
 */
 
int main(int argc,char **argv) {

  signal(SIGINT,_generic_rcvsig);
  
  // Prepare the framebuffer.
  if ((gamek_client.fbw>0)&&(gamek_client.fbh>0)) {
    gamek_generic.fb.w=gamek_client.fbw;
    gamek_generic.fb.h=gamek_client.fbh;
  } else {
    gamek_generic.fb.w=96;
    gamek_generic.fb.h=64;
  }
  gamek_generic.fb.stride=gamek_generic.fb.w<<2;
  gamek_generic.fb.fmt=GAMEK_IMGFMT_RGBX;
  gamek_generic.fb.flags=GAMEK_IMAGE_FLAG_WRITEABLE;
  if (!(gamek_generic.fb.v=malloc(gamek_generic.fb.stride*gamek_generic.fb.h))) return 1;
  
  if (gamek_client.init) {
    if (gamek_client.init()<0) {
      fprintf(stderr,"%s: client init failed\n",argv[0]);
      return 1;
    }
  }
  
  // Simulate connecting four joysticks.
  if (gamek_client.input_event) {
    gamek_client.input_event(1,GAMEK_BUTTON_CD,1);
    gamek_client.input_event(2,GAMEK_BUTTON_CD,1);
    gamek_client.input_event(3,GAMEK_BUTTON_CD,1);
    gamek_client.input_event(4,GAMEK_BUTTON_CD,1);
    gamek_client.input_event(0,GAMEK_BUTTON_CD,1);
  }
  
  //TODO Gather statistics and timing.
  
  fprintf(stderr,"%s: Running. SIGINT to quit.\n",argv[0]);
  while (!gamek_generic.terminate) {
  
    usleep(20000);//TODO timing
    
    //TODO Simulate input from a user script?
    
    if (gamek_client.update) {
      gamek_client.update();
    }
    
    if (gamek_client.render) {
      if (gamek_client.render(&gamek_generic.fb)) {
        //TODO Option to grab screencaps?
      }
    }
  }
  
  fprintf(stderr,"%s: Normal exit.\n",argv[0]);
  return 0;
}
