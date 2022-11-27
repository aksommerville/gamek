#include "gamek_inmgr_internal.h"
#include "opt/serial/serial.h"

/* Button names.
 */
 
static const char *gamek_inmgr_button_names[16]={
  "LEFT",
  "RIGHT",
  "UP",
  "DOWN",
  "SOUTH",
  "WEST",
  "EAST",
  "NORTH",
  "L1",
  "R1",
  "L2",
  "R2",
  "AUX1",
  "AUX2",
  "AUX3",
  0, // CD; not encodable
};

static const char *gamek_inmgr_action_names[]={
#define _(tag) [GAMEK_ACTION_##tag]=#tag,
  _(QUIT)
  _(FULLSCREEN)
#undef _
};

static int gamek_inmgr_button_repr(struct sr_encoder *dst,int btnid) {
  if ((btnid>0)&&(btnid<0x10000)) {
    int ix=0;
    int q=btnid;
    while (!(q&1)) { q>>=1; ix++; }
    if (q==1) {
      const char *name=gamek_inmgr_button_names[ix];
      if (name) {
        return sr_encode_raw(dst,name,-1);
      }
    }
  }
  return sr_encode_fmt(dst,"%d",btnid);
}

static int gamek_inmgr_button_eval(const char *src,int srcc) {
  int ix=0; for (;ix<16;ix++) {
    const char *q=gamek_inmgr_button_names[ix];
    if (!q) continue;
    if (memcmp(q,src,srcc)) continue;
    if (q[srcc]) continue;
    return 1<<ix;
  }
  int v;
  if (sr_int_eval(&v,src,srcc)<2) return -1;
  return v;
}

static int gamek_inmgr_action_repr(struct sr_encoder *dst,int actionid) {
  int namec=sizeof(gamek_inmgr_action_names)/sizeof(void*);
  if ((actionid>=0)&&(actionid<namec)) {
    const char *name=gamek_inmgr_action_names[actionid];
    if (name) {
      return sr_encode_raw(dst,name,-1);
    }
  }
  return sr_encode_fmt(dst,"%d",actionid);
}

static int gamek_inmgr_action_eval(const char *src,int srcc) {
  int namec=sizeof(gamek_inmgr_action_names)/sizeof(void*);
  int actionid=0;
  for (;actionid<namec;actionid++) {
    const char *q=gamek_inmgr_action_names[actionid];
    if (!q) continue;
    if (memcmp(q,src,srcc)) continue;
    if (q[srcc]) continue;
    return actionid;
  }
  if (sr_int_eval(&actionid,src,srcc)<2) return -1;
  return actionid;
}

/* Encode one template.
 */
 
static int gamek_inmgr_tm_encode(struct sr_encoder *dst,const struct gamek_inmgr_tm *tm) {
  if (sr_encode_fmt(dst,">>> 0x%04x 0x%04x %s\n",tm->vid,tm->pid,tm->name?tm->name:"")<0) return -1;
  const struct gamek_inmgr_tm_btn *btn=tm->btnv;
  int btni=tm->btnc;
  for (;btni-->0;btn++) {
    if (sr_encode_fmt(dst,"0x%08x ",btn->srcbtnid)<0) return -1;
    
    if (btn->flags&GAMEK_INMGR_TM_BTN_FLAG_ACTION) {
      if (gamek_inmgr_action_repr(dst,btn->dstbtnid)<0) return -1;
    } else {
      if (gamek_inmgr_button_repr(dst,btn->dstbtnid)<0) return -1;
    }
    
    if (sr_encode_fmt(dst," %d %d",btn->srclo,btn->srchi)<0) return -1;
    
    if (btn->flags) {
      #define _(tag) if (btn->flags&GAMEK_INMGR_TM_BTN_FLAG_##tag) { \
        if (sr_encode_raw(dst," ",1)<0) return -1; \
        if (sr_encode_raw(dst,#tag,-1)<0) return -1; \
      }
      _(ACTION)
      _(LOWPRIORITY)
      _(PLAYER2)
      _(USAGE)
      _(CRITICAL)
      #undef _
    }
    
    if (sr_encode_raw(dst,"\n",1)<0) return -1;
  }
  return 0;
}

/* Encode configuration.
 */
 
static int gamek_inmgr_encode_configuration_inner(
  struct sr_encoder *dst,
  struct gamek_inmgr *inmgr
) {
  if (sr_encode_raw(dst,
    "# Input configuration.\n"
    "# Beware: This file may be overwritten automatically.\n"
    "# Comments and formatting will not be preserved.\n"
  ,-1)<0) return -1;
  
  const struct gamek_inmgr_tm *tm=inmgr->tmv;
  int i=inmgr->tmc;
  for (;i-->0;tm++) {
    if (gamek_inmgr_tm_encode(dst,tm)<0) return -1;
  }
  
  return 0;
}
 
int gamek_inmgr_encode_configuration(void *dstpp,struct gamek_inmgr *inmgr) {
  if (!dstpp||!inmgr) return -1;
  struct sr_encoder encoder={0};
  if (gamek_inmgr_encode_configuration_inner(&encoder,inmgr)<0) {
    sr_encoder_cleanup(&encoder);
    return -1;
  }
  *(void**)dstpp=encoder.v;
  return encoder.c;
}

/* Decode one rule.
 * SRCBTNID DSTBTNID LO HI FLAGS
 */
 
static int gamek_inmgr_decode_rule(struct gamek_inmgr *inmgr,struct gamek_inmgr_tm *tm,const char *src,int srcc) {
  const char *token;
  int srcp=0,tokenc;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  #define NEXTTOKEN { \
    token=src+srcp; \
    tokenc=0; \
    while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; } \
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++; \
  }
  
  // SRCBTNID
  NEXTTOKEN
  int srcbtnid;
  if (sr_int_eval(&srcbtnid,token,tokenc)<1) return -1;
  
  // DSTBTNID. Don't evaluate until we've seen FLAGS.
  NEXTTOKEN
  const char *dstbtnidsrc=token;
  int dstbtnidsrcc=tokenc;
  
  // LO HI
  int lo,hi;
  NEXTTOKEN
  if (sr_int_eval(&lo,token,tokenc)<2) return -1;
  NEXTTOKEN
  if (sr_int_eval(&hi,token,tokenc)<2) return -1;
  
  // FLAGS
  uint8_t flags=0;
  while (srcp<srcc) {
    NEXTTOKEN
    #define _(tag) if ((tokenc==sizeof(#tag)-1)&&!memcmp(token,#tag,tokenc)) flags|=GAMEK_INMGR_TM_BTN_FLAG_##tag; else
    _(ACTION)
    _(LOWPRIORITY)
    _(PLAYER2)
    _(USAGE)
    _(CRITICAL)
    return -1;
    #undef _
  }
  
  #undef NEXTTOKEN
  if (srcp<srcc) return -1;
  
  // Now evaluate DSTBTNID.
  int dstbtnid;
  if (flags&GAMEK_INMGR_TM_BTN_FLAG_ACTION) {
    if ((dstbtnid=gamek_inmgr_action_eval(dstbtnidsrc,dstbtnidsrcc))<0) return -1;
  } else {
    if ((dstbtnid=gamek_inmgr_button_eval(dstbtnidsrc,dstbtnidsrcc))<0) {
      // Allow that the ACTION flag was omitted. If it names an action, go with it.
      if ((dstbtnid=gamek_inmgr_action_eval(dstbtnidsrc,dstbtnidsrcc))>=0) {
        flags|=GAMEK_INMGR_TM_BTN_FLAG_ACTION;
      } else {
        return -1;
      }
    }
  }
  
  // And add it.
  struct gamek_inmgr_tm_btn *btn=gamek_inmgr_tm_btnv_append(tm);
  if (!btn) return -1;
  btn->srcbtnid=srcbtnid;
  btn->dstbtnid=dstbtnid;
  btn->srclo=lo;
  btn->srchi=hi;
  btn->flags=flags;

  return 0;
}

/* Decode device introducer.
 * The leading ">>>" has been stripped.
 */
 
static struct gamek_inmgr_tm *gamek_inmgr_decode_block_start(struct gamek_inmgr *inmgr,const char *src,int srcc) {
  int vid=0,pid=0,namec=0;
  const char *name=0;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *token=src+srcp;
  int tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  if (sr_int_eval(&vid,token,tokenc)<2) return 0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  if (sr_int_eval(&pid,token,tokenc)<2) return 0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  name=src+srcp;
  namec=srcc-srcp;
  return gamek_inmgr_tm_new(inmgr,name,namec,vid,pid);
}

/* Configure from text.
 */

int gamek_inmgr_configure_text(struct gamek_inmgr *inmgr,const char *src,int srcc,const char *refname) {
  if (!inmgr||inmgr->ready) return -1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=0;
  struct gamek_inmgr_tm *tm=0;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    if (line[0]=='#') continue;
    
    if ((linec>=3)&&!memcmp(line,">>>",3)) {
      line+=3;
      linec-=3;
      if (!(tm=gamek_inmgr_decode_block_start(inmgr,line,linec))) {
        if (refname) fprintf(stderr,"%s:%d: Invalid device block introducer.\n",refname,lineno);
        return -1;
      }
    } else if (!tm) {
      if (refname) fprintf(stderr,"%s:%d: Expected device block introducer before button rule.\n",refname,lineno);
      return -1;
    } else {
      if (gamek_inmgr_decode_rule(inmgr,tm,line,linec)<0) {
        if (refname) fprintf(stderr,"%s:%d: Failed to decode button rule.\n",refname,lineno);
        return -1;
      }
    }
  }
  return 0;
}
