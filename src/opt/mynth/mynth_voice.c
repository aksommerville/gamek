#include "mynth_internal.h"

/* Update.
 */
 
void mynth_voice_update(int16_t *v,uint16_t c,struct mynth_voice *voice) {
  if (!voice->v) return;
  
  uint16_t i=c;
  for (;i-->0;v++) {
    
    int16_t sample=voice->v[voice->p>>MYNTH_P_SHIFT];
    voice->p+=voice->dp;
    
    if (voice->ddp) {
      voice->dp+=voice->ddp;
      if ((voice->dp>=voice->dphi)&&voice->dphi) {
        if (voice->dplo) voice->ddp=-voice->ddp;
        else voice->ddp=0;
      } else if (voice->dp<=voice->dplo) {
        if (voice->dphi) voice->ddp=-voice->ddp;
        else voice->ddp=0;
      }
    }
    
    uint32_t level=voice->env.v>>8; // 0..0xffff
    sample=(sample*level)>>16;
    
    if (voice->env.hold) {
    } else if (voice->env.c) {
      voice->env.c--;
      voice->env.v+=voice->env.d;
    } else {
      if (voice->env.pointv[0].c) {
        voice->env.v=voice->env.pointv[0].v;
        voice->env.c=voice->env.pointv[1].c;
        voice->env.d=(voice->env.pointv[1].v-voice->env.v)/voice->env.c;
        voice->env.pointv[0].c=0;
      } else if (voice->env.pointv[1].c) {
        voice->env.v=voice->env.pointv[1].v;
        voice->env.c=voice->env.pointv[2].c;
        voice->env.d=(voice->env.pointv[2].v-voice->env.v)/voice->env.c;
        voice->env.pointv[1].c=0;
        if (voice->env.sustain) voice->env.hold=1;
      } else {
        voice->v=0;
        return;
      }
    }
    
    (*v)+=sample;
  }
  
  if (voice->ttl) {
    if (voice->ttl>c) voice->ttl-=c;
    else {
      voice->ttl=0;
      mynth_voice_release(voice);
    }
  }
}

/* Release.
 */
 
void mynth_voice_release(struct mynth_voice *voice) {
  voice->chid=0xff;
  voice->noteid=0xff;
  voice->env.sustain=0;
  if (voice->env.hold) voice->env.hold=0;
}

/* Begin.
 */
 
void mynth_voice_begin(struct mynth_voice *voice,struct mynth_channel *channel,uint8_t noteid,uint8_t velocity) {

  if (!channel->wave) {
    voice->v=0;
    return;
  }

  voice->v=channel->wave;
  voice->dp=mynth.ratev[noteid&0x7f]*channel->bend;
  voice->p=0;
  voice->ttl=0;
  voice->chid=channel-mynth.channelv;
  voice->noteid=noteid;
  
  if (channel->warble_range&&channel->warble_rate) {
    float adjust=powf(2.0f,channel->warble_range/120.0f);
    voice->dphi=voice->dp*adjust;
    voice->dplo=voice->dp/adjust;
    adjust=(float)channel->warble_rate/(float)mynth.rate;
    voice->ddp=((int32_t)voice->dplo-(int32_t)voice->dphi)*adjust;
  } else {
    voice->dplo=0;
    voice->dphi=0;
    voice->ddp=0;
  }
  
  mynth_channel_generate_envelope(&voice->env,channel,velocity);
}

/* Reapply wheel.
 */
 
void mynth_voice_reapply_wheel(struct mynth_voice *voice,struct mynth_channel *channel) {
  if (voice->noteid>=0x80) return;
  voice->dp=mynth.ratev[voice->noteid&0x7f]*channel->bend;
}
