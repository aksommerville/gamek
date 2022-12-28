#include "mynth_internal.h"

/* Reset channel.
 */
 
void mynth_channel_reset(struct mynth_channel *channel) {
  channel->volume=0x40;
  channel->wheel=0x2000;
  channel->bend=1.0f;
  channel->wheel_range=20;
  channel->waveid=0;
  channel->wave=mynth.wavev[0];
  channel->sustain=1;
  channel->warble_range=0;
  channel->warble_rate=0;
  
  // Initial envelope has sensible defaults and no velocity sensitivity.
  channel->atkclo=channel->atkchi=(mynth.rate*  15)/1000;
  channel->decclo=channel->decchi=(mynth.rate*  40)/1000;
  channel->rlsclo=channel->rlschi=(mynth.rate* 200)/1000;
  channel->atkvlo=channel->atkvhi=0x500000;
  channel->susvlo=channel->susvhi=0x300000;
}

/* Control Change.
 */
 
void mynth_channel_control(struct mynth_channel *channel,uint8_t k,uint8_t v) {
  switch (k) {
    case 0x07: channel->volume=v; return;
    
    case 0x0c: channel->atkclo=channel->atkchi=(v*mynth.rate)/1000+1; break;
    case 0x0d: channel->atkvlo=channel->atkvhi=(v<<16)|(v<<8)|v; break; // NB (v<<17) to use the full range
    case 0x0e: channel->decclo=channel->decchi=(v*mynth.rate)/1000+1; break;
    case 0x0f: channel->susvlo=channel->susvhi=(v<<16)|(v<<8)|v; break;
    case 0x10: channel->rlsclo=channel->rlschi=(v*8*mynth.rate)/1000+1; break;
    case 0x2c: channel->atkchi=(v*mynth.rate)/1000+1; break;
    case 0x2d: channel->atkvhi=(v<<16)|(v<<8)|v; break;
    case 0x2e: channel->decchi=(v*mynth.rate)/1000+1; break;
    case 0x2f: channel->susvhi=(v<<16)|(v<<8)|v; break;
    case 0x30: channel->rlschi=(v*8*mynth.rate)/1000+1; break;
    
    case 0x40: channel->sustain=(v>=0x40)?1:0; break;
    case 0x46: channel->waveid=v&7; channel->wave=mynth.wavev[channel->waveid]; break;
    case 0x47: channel->wheel_range=v; break;
    case 0x48: channel->warble_range=v; break;
    case 0x49: channel->warble_rate=v; break;
    case 0x4a: channel->warble_phase=v; break;
    
    case 0x78: mynth_stop_voices_for_channel(channel-mynth.channelv); return;
    case 0x79: mynth_channel_reset(channel); return;
    case 0x7b: mynth_release_voices_for_channel(channel-mynth.channelv); return;
  }
}

/* Begin envelope.
 */
 
void mynth_channel_generate_envelope(struct mynth_env *env,const struct mynth_channel *channel,uint8_t velocity) {
  
  // Interpolate between channel's "lo" and "hi" according to velocity.
  int32_t atkc,atkv,decc,susv,rlsc;
  if (velocity>=0x7f) {
    atkc=channel->atkchi;
    atkv=channel->atkvhi;
    decc=channel->decchi;
    susv=channel->susvhi;
    rlsc=channel->rlschi;
  } else if (!velocity) {
    atkc=channel->atkclo;
    atkv=channel->atkvlo;
    decc=channel->decclo;
    susv=channel->susvlo;
    rlsc=channel->rlsclo;
  } else {
    uint8_t loweight=0x7f-velocity;
    atkc=(channel->atkclo*loweight+channel->atkchi*velocity)>>7;
    atkv=(channel->atkvlo*loweight+channel->atkvhi*velocity)>>7;
    decc=(channel->decclo*loweight+channel->decchi*velocity)>>7;
    susv=(channel->susvlo*loweight+channel->susvhi*velocity)>>7;
    rlsc=(channel->rlsclo*loweight+channel->rlschi*velocity)>>7;
  }
  
  // Apply channel volume to levels.
  atkv=(atkv*channel->volume)>>7;
  susv=(susv*channel->volume)>>7;
  
  // Force valid. Times must be at least one, and levels at least zero.
  if (atkc<1) atkc=1;
  if (atkv<0) atkv=0;
  if (decc<1) decc=1;
  if (susv<0) susv=0;
  if (rlsc<1) rlsc=1;
  
  // Write to the 3 points of the envelope runner.
  env->pointv[0].v=atkv;
  env->pointv[0].c=atkc;
  env->pointv[1].v=susv;
  env->pointv[1].c=decc;
  env->pointv[2].v=0;
  env->pointv[2].c=rlsc;
  
  // And finally, set the runner's initial state.
  env->v=0;
  env->c=env->pointv[0].c;
  env->d=env->pointv[0].v/env->c;
  env->hold=0;
  env->sustain=channel->sustain;
}
