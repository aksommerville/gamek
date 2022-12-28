#include "fiddle_internal.h"
#include "opt/argv/gamek_argv.h"
#include "opt/serial/serial.h"
#include <signal.h>

struct fiddle fiddle={0};

/* Lock audio driver.
 * We tolerate redundant lock and unlock.
 */
 
int fiddle_lock() {
  if (fiddle.lockc) return 0;
  #if GAMEK_USE_alsapcm
    if (!fiddle.alsapcm) return 0;
    if (alsapcm_lock(fiddle.alsapcm)<0) return -1;
    fiddle.lockc=1;
    return 0;
  #endif
  return 0;
}

void fiddle_unlock() {
  if (!fiddle.lockc) return;
  fiddle.lockc=0;
  #if GAMEK_USE_alsapcm
    alsapcm_unlock(fiddle.alsapcm);
  #endif
}

/* Expand mono to whatever, in place.
 */
 
static void fiddle_expand_channels(int16_t *v,int framec,int dstchanc) {
  if (dstchanc==2) { // likely enough to warrant special handling
    const int16_t *srcp=v+framec;
    int16_t *dstp=v+(framec<<1);
    while (framec-->0) {
      srcp--;
      dstp-=2;
      dstp[0]=dstp[1]=*srcp;
    }
  } else {
    const int16_t *srcp=v+framec;
    int16_t *dstp=v+framec*dstchanc;
    while (framec-->0) {
      srcp--;
      int i=dstchanc; while (i-->0) {
        dstp--;
        *dstp=*srcp;
      }
    }
  }
}

/* Generate PCM.
 */
 
static void fiddle_pcm_out(int16_t *v,int c,void *userdata) {
  #if GAMEK_USE_mynth
    if (fiddle.chanc>1) {
      int framec=c/fiddle.chanc;
      mynth_update(v,framec);
      fiddle_expand_channels(v,framec,fiddle.chanc);
    } else {
      mynth_update(v,c);
    }
  #else
    memset(v,0,c<<1);
  #endif
  while (c&&fiddle.ragec) {
    (*v)+=(rand()&0x3fff)-0x2000;
    c--;
    v++;
    fiddle.ragec--;
  }
}

/* Rage.
 */
 
void fiddle_unleash_rage() {
  #if GAMEK_USE_alsapcm
    fiddle.ragec=(alsapcm_get_rate(fiddle.alsapcm)*alsapcm_get_chanc(fiddle.alsapcm))/2;
  #endif
}

/* Receive content.
 * alsamidi always sends one event per callback, but other APIs might not.
 * Allow that it could be multiple events.
 * No need to bother keeping partial events, if they're split across callbacks.
 * We'll make sure that can't happen at the driver levels.
 */
 
static void fiddle_midi_in(const void *v,int c,void *userdata) {
  if (fiddle_lock()<0) return;
  #if 0
    // Fancier synthesizers will likely accept serial MIDI. If that's so, just shovel it through.
  #else
    // For mynth and maybe others, we have to digest MIDI events.
    const uint8_t *V=v;
    while (c>0) {
      uint8_t lead=*(V++); c--;
      uint8_t opcode=lead&0xf0,chid=lead&0x0f,a=0,b=0;
      switch (opcode) {
        #define AB if (c) { a=*(V++); c--; if (c) { b=*(V++); c--; } }
        #define A if (c) { a=*(V++); c--; }
        case 0x80: AB break;
        case 0x90: AB break;
        case 0xa0: AB break;
        case 0xb0: AB break;
        case 0xc0: A break;
        case 0xd0: A break;
        case 0xe0: AB break;
        #undef AB
        #undef A
        case 0xf0: opcode=0; switch (lead) {
            case 0xff: chid=0xff; opcode=0xff; break;
          } break;
        default: opcode=0;
      }
      if (opcode) {
        //fprintf(stderr,"EVENT: %02x %02x %02x %02x\n",chid,opcode,a,b);
        #if GAMEK_USE_mynth
          mynth_event(chid,opcode,a,b);
        #endif
      }
    }
  #endif
}

/* Signals.
 */
 
static void fiddle_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(fiddle.terminate)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Quit.
 */
 
static void fiddle_quit() {

  #if GAMEK_USE_alsamidi
    alsamidi_del(fiddle.alsamidi);
  #endif
  
  #if GAMEK_USE_alsapcm
    alsapcm_del(fiddle.alsapcm);
  #endif
  
  #if GAMEK_USE_mynth
    // no cleanup necessary
  #endif
}

/* Init drivers.
 */
 
static int fiddle_init_drivers() {
  
  // MIDI-In.
  #if GAMEK_USE_alsamidi
    struct alsamidi_delegate alsamidi_delegate={
      .cb=fiddle_midi_in,
    };
    if (!(fiddle.alsamidi=alsamidi_new(0,"Gamek Fiddle",&alsamidi_delegate))) {
      fprintf(stderr,"alsamidi_new failed\n");
      return -2;
    }
  #else
    fprintf(stderr,"fiddle:WARNING: No MIDI-In driver. Please enable one: alsamidi\n");
  #endif
  
  // PCM-Out.
  #if GAMEK_USE_alsapcm
    struct alsapcm_delegate alsapcm_delegate={
      .pcm_out=fiddle_pcm_out,
    };
    struct alsapcm_setup alsapcm_setup={
      .rate=44100,
      .chanc=1,
    };
    if (!(fiddle.alsapcm=alsapcm_new(&alsapcm_delegate,&alsapcm_setup))) {
      fprintf(stderr,"alsapcm_new failed\n");
      return -2;
    }
    fiddle.rate=alsapcm_get_rate(fiddle.alsapcm);
    fiddle.chanc=alsapcm_get_chanc(fiddle.alsapcm);
  #else
    fprintf(stderr,"fiddle:WARNING: No PCM-Out driver. Please enable one: alsapcm\n");
  #endif
  
  // Synth.
  #if GAMEK_USE_mynth
    mynth_init(fiddle.rate);
  #else
    fprintf(stderr,"fiddle:WARNING: No synthesizer. Please enable one: mynth\n");
  #endif
  
  // Start PCM.
  #if GAMEK_USE_alsapcm
    alsapcm_set_running(fiddle.alsapcm,1);
  #endif
  
  return 0;
}

/* Receive argument.
 */
 
static void fiddle_print_help(const char *topic,int topicc) {
  fprintf(stderr,
    "\n"
    "Usage: fiddle [OPTIONS]\n"
    "\n"
    "OPTIONS:\n"
    "  --wave-N=PATH       Compile and load a wave file, and monitor it for changes.\n"
    "\n"
  );
}
 
static int fiddle_cb_arg(const char *k,int kc,const char *v,int vc,int vn,void *userdata) {

  if ((kc==4)&&!memcmp(k,"help",4)) {
    fiddle_print_help(v,vc);
    return 1;
  }
  
  if ((kc>5)&&!memcmp(k,"wave-",5)) {
    int waveid=-1;
    if ((sr_int_eval(&waveid,k+5,kc-5)<2)||(waveid<0)||(waveid>7)) {
      fprintf(stderr,"fiddle:ERROR: Invalid wave ID: '%.*s'\n",kc,k);
      return -2;
    }
    int err=fiddle_set_wave(waveid,v,vc);
    if (err<0) return err;
    return 0;
  }

  fprintf(stderr,"fiddle:ERROR: Unexpected argument '%.*s'='%.*s'\n",kc,k,vc,v);
  return -2;
}

/* Init. Returns 0 to proceed, -2 if an error was logged, <0 to fail, >0 to terminate successfully.
 */
 
static int fiddle_init(int argc,char **argv) {

  signal(SIGINT,fiddle_rcvsig);
  
  int err=fiddle_waves_init();
  if (err<0) return err;
  
  if (err=gamek_argv_read(argc,argv,fiddle_cb_arg,0)) return err;
  
  if ((err=fiddle_init_drivers())<0) return err;
  
  return 0;
}

/* Update.
 */
 
static int fiddle_update() {

  int err=fiddle_waves_update();
  if (err<0) {
    if (err!=-2) fprintf(stderr,"Unspecified error updating waves.\n");
    return -2;
  }

  #if GAMEK_USE_alsamidi
    if (alsamidi_update(fiddle.alsamidi,100)<0) {
      fprintf(stderr,"alsamidi_update failed\n");
      return -2;
    }
  #else
    usleep(100000);
  #endif
  
  #if GAMEK_USE_alsapcm
    if (alsapcm_update(fiddle.alsapcm)<0) {
      fprintf(stderr,"alsapcm_update failed\n");
      return -2;
    }
  #endif
  
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  if (err=fiddle_init(argc,argv)) {
    if ((err<0)&&(err!=-2)) fprintf(stderr,"fiddle_init failed\n");
    return (err<0)?1:0;
  }
  fprintf(stderr,"Fiddle running. Connect via MIDI bus. SIGINT to quit.\n");
  while (!fiddle.terminate) {
    err=fiddle_update();
    fiddle_unlock();
    if (err<0) {
      if (err!=-2) fprintf(stderr,"fiddle_update failed\n");
      fiddle_quit();
      return 1;
    }
  }
  fiddle_quit();
  return 0;
}
