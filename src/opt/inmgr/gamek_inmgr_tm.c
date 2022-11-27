#include "gamek_inmgr_internal.h"

/* Cleanup.
 */
 
void gamek_inmgr_tm_cleanup(struct gamek_inmgr_tm *tm) {
  if (tm->name) free(tm->name);
  if (tm->btnv) free(tm->btnv);
}

/* Test device.
 */
 
int gamek_inmgr_tm_match(
  const struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_greeting *greeting
) {
  if (!tm||!greeting) return 0;
  
  // Check IDs first because they're easier.
  // If nonzero in the template, they must match exactly.
  if (tm->vid&&(tm->vid!=greeting->vid)) return 0;
  if (tm->pid&&(tm->pid!=greeting->pid)) return 0;
  
  // Name is a little more interesting.
  if (tm->name&&tm->name[0]) {
    int namec=0;
    if (greeting->name) { while (greeting->name[namec]) namec++; }
    if (!namec) return 0;
    char norm[256];
    if (namec>sizeof(norm)) namec=sizeof(norm); // hopefully the important part is in the first 256...
    int subc=0; while (tm->name[subc]) subc++;
    if (subc<=namec) {
      // Normalize device name. Lowercase letters, keep letters and digits, force all punctuation to spaces.
      int i=namec; while (i-->0) {
             if ((greeting->name[i]>='A')&&(greeting->name[i]<='Z')) norm[i]=greeting->name[i]+0x20;
        else if ((greeting->name[i]>='a')&&(greeting->name[i]<='z')) norm[i]=greeting->name[i];
        else if ((greeting->name[i]>='0')&&(greeting->name[i]<='9')) norm[i]=greeting->name[i];
        else norm[i]=' ';
      }
      // Now a match of (tm->name) anywhere in (norm) is a match.
      int matched=0;
      int limit=namec-subc;
      int normp=0; for (;normp<=limit;normp++) {
        if (!memcmp(norm+normp,tm->name,subc)) {
          matched=1;
          break;
        }
      }
      if (!matched) return 0;
    }
  }
  
  /* Consider the overlap of device capabilities and mapped buttons.
   * If the template has a CRITICAL button that isn't present on device, mismatch.
   * If the template has no button rules, match. Maybe it's some non-joystick device being used for actions only.
   * If we don't have buttons for the dpad and the first two thumb buttons, mismatch.
   */
  int has_actions=0,has_buttons=0;
  const uint16_t required_buttons=GAMEK_BUTTON_HAT|GAMEK_BUTTON_SOUTH|GAMEK_BUTTON_WEST;
  uint16_t okbuttons=0;
  const struct gamek_inmgr_tm_btn *tmbtn=tm->btnv;
  int tmbtni=tm->btnc;
  for (;tmbtni-->0;tmbtn++) {
    if (tmbtn->flags&GAMEK_INMGR_TM_BTN_FLAG_ACTION) has_actions=1;
    else {
      has_buttons=1;
      const struct gamek_inmgr_joystick_capability *cap=0;
      if (tmbtn->flags&GAMEK_INMGR_TM_BTN_FLAG_USAGE) {
        cap=gamek_inmgr_joystick_greeting_get_capability_by_usage(greeting,tmbtn->srcbtnid);
      } else {
        cap=gamek_inmgr_joystick_greeting_get_capability(greeting,tmbtn->srcbtnid);
      }
      if (cap) {
        okbuttons|=tmbtn->dstbtnid;
      } else if (tmbtn->flags&GAMEK_INMGR_TM_BTN_FLAG_CRITICAL) {
        return 0;
      }
    }
  }
  if (has_actions&&!has_buttons) {
    // action-only device, that's ok.
  } else if ((okbuttons&required_buttons)!=required_buttons) {
    return 0;
  }
  
  // OK it matches.
  return 1;
}

/* Append template to inmgr.
 */
 
struct gamek_inmgr_tm *gamek_inmgr_tm_new(
  struct gamek_inmgr *inmgr,
  const char *name,int namec,
  uint16_t vid,uint16_t pid
) {
  if (inmgr->tmc>=inmgr->tma) {
    int na=inmgr->tma+16;
    if (na>INT_MAX/sizeof(struct gamek_inmgr_tm)) return 0;
    void *nv=realloc(inmgr->tmv,sizeof(struct gamek_inmgr_tm)*na);
    if (!nv) return 0;
    inmgr->tmv=nv;
    inmgr->tma=na;
  }
  struct gamek_inmgr_tm *tm=inmgr->tmv+inmgr->tmc++;
  memset(tm,0,sizeof(struct gamek_inmgr_tm));
  tm->vid=vid;
  tm->pid=pid;
  
  if (name) {
    if (namec<0) { namec=0; while (name[namec]) namec++; }
    if (namec) {
      if (!(tm->name=malloc(namec+1))) return 0;
      memcpy(tm->name,name,namec);
      tm->name[namec]=0;
    }
  }
  
  return tm;
}

/* Add a provisional template.
 * They get added to the front of the list, to avoid going behind some match-all default.
 */
 
static struct gamek_inmgr_tm *gamek_inmgr_tm_add(struct gamek_inmgr *inmgr) {
  if (inmgr->tmc>=inmgr->tma) {
    int na=inmgr->tma+16;
    if (na>INT_MAX/sizeof(struct gamek_inmgr_tm)) return 0;
    void *nv=realloc(inmgr->tmv,sizeof(struct gamek_inmgr_tm)*na);
    if (!nv) return 0;
    inmgr->tmv=nv;
    inmgr->tma=na;
  }
  struct gamek_inmgr_tm *tm=inmgr->tmv;
  memmove(tm+1,tm,sizeof(struct gamek_inmgr_tm)*inmgr->tmc);
  inmgr->tmc++;
  memset(tm,0,sizeof(struct gamek_inmgr_tm));
  return tm;
}

/* Remove and cleanup a template.
 */
 
static void gamek_inmgr_tm_remove(struct gamek_inmgr *inmgr,struct gamek_inmgr_tm *tm) {
  int p=tm-inmgr->tmv;
  if ((p<0)||(p>=inmgr->tmc)) return;
  gamek_inmgr_tm_cleanup(tm);
  inmgr->tmc--;
  memmove(tm,tm+1,sizeof(struct gamek_inmgr_tm)*(inmgr->tmc-p));
}

/* Add button to template.
 */
 
struct gamek_inmgr_tm_btn *gamek_inmgr_tm_btnv_append(struct gamek_inmgr_tm *tm) {
  if (tm->btnc>=tm->btna) {
    int na=tm->btna+16;
    if (na>INT_MAX/sizeof(struct gamek_inmgr_tm_btn)) return 0;
    void *nv=realloc(tm->btnv,sizeof(struct gamek_inmgr_tm_btn)*na);
    if (!nv) return 0;
    tm->btnv=nv;
    tm->btna=na;
  }
  struct gamek_inmgr_tm_btn *btn=tm->btnv+tm->btnc++;
  memset(btn,0,sizeof(struct gamek_inmgr_tm_btn));
  return btn;
}

/* Extra context for template synthesis.
 */

// Important that these match GAMEK_BUTTON_*
#define GAMEK_BTNIX_LEFT     0
#define GAMEK_BTNIX_RIGHT    1
#define GAMEK_BTNIX_UP       2
#define GAMEK_BTNIX_DOWN     3
#define GAMEK_BTNIX_SOUTH    4
#define GAMEK_BTNIX_WEST     5
#define GAMEK_BTNIX_EAST     6
#define GAMEK_BTNIX_NORTH    7
#define GAMEK_BTNIX_L1       8
#define GAMEK_BTNIX_R1       9
#define GAMEK_BTNIX_L2      10
#define GAMEK_BTNIX_R2      11
#define GAMEK_BTNIX_AUX1    12
#define GAMEK_BTNIX_AUX2    13
#define GAMEK_BTNIX_AUX3    14
#define GAMEK_BTNIX_CD      15
 
struct gamek_inmgr_tmsyn_context {
  uint16_t buttons;
  int count_by_btnid[16];
};

static int gamek_inmgr_tmsyn_context_valid(
  const struct gamek_inmgr_tmsyn_context *ctx
) {
  const uint16_t required_buttons=GAMEK_BUTTON_HAT|GAMEK_BUTTON_SOUTH|GAMEK_BUTTON_WEST;
  if ((ctx->buttons&required_buttons)!=required_buttons) return 0;
  return 1;
}

/* Add a twostate button with an explicit output.
 */
 
static int gamek_inmgr_tm_add_twostate(
  struct gamek_inmgr_tm *tm,
  int srcbtnid,
  struct gamek_inmgr_tmsyn_context *ctx,
  uint16_t dstbtnid
) {
  if (!dstbtnid) return -1;
  int ix=0;
  uint16_t q=dstbtnid;
  while (!(q&1)) { ix++; q>>=1; }
  if (q!=1) return -1; // not a single bit!
  struct gamek_inmgr_tm_btn *btn=gamek_inmgr_tm_btnv_append(tm);
  if (!btn) return -1;
  btn->srcbtnid=srcbtnid;
  btn->srclo=1;
  btn->srchi=INT_MAX;
  btn->flags=0;
  btn->dstbtnid=dstbtnid;
  ctx->buttons|=dstbtnid;
  ctx->count_by_btnid[ix]++;
  return 0;
}

/* Add an action from a twostate button.
 */
 
static int gamek_inmgr_tm_add_action(
  struct gamek_inmgr_tm *tm,
  int srcbtnid,
  struct gamek_inmgr_tmsyn_context *ctx,
  uint16_t actionid
) {
  struct gamek_inmgr_tm_btn *btn=gamek_inmgr_tm_btnv_append(tm);
  if (!btn) return -1;
  btn->srcbtnid=srcbtnid;
  btn->srclo=1;
  btn->srchi=INT_MAX;
  btn->flags=GAMEK_INMGR_TM_BTN_FLAG_ACTION;
  btn->dstbtnid=actionid;
  return 0;
}

/* Add a threestate axis.
 */
 
static int gamek_inmgr_tm_add_axis(
  struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_capability *cap,
  struct gamek_inmgr_tmsyn_context *ctx,
  uint16_t dstbtnid
) {
  if ((dstbtnid!=GAMEK_BUTTON_HORZ)&&(dstbtnid!=GAMEK_BUTTON_VERT)) return -1;
  
  // The OFF range should be 1/3 of the full range, and must not be empty.
  int range=cap->hi-cap->lo+1;
  if (range<3) return -1;
  int midlen=range/3;
  int midlo=cap->lo+midlen;
  int midhi=cap->hi-midlen;
  // These should be impossible due to prior assertions, but I'm not 1000% on it:
  if (midlo<=cap->lo) return -1;
  if (midhi>=cap->hi) return -1;
  if (midlo>midhi) return -1;
  
  struct gamek_inmgr_tm_btn *btn=gamek_inmgr_tm_btnv_append(tm);
  if (!btn) return -1;
  btn->srcbtnid=cap->btnid;
  btn->srclo=INT_MIN;
  btn->srchi=midlo-1;
  btn->flags=0;
  btn->dstbtnid=(dstbtnid&(GAMEK_BUTTON_LEFT|GAMEK_BUTTON_UP));
  
  if (!(btn=gamek_inmgr_tm_btnv_append(tm))) return -1;
  btn->srcbtnid=cap->btnid;
  btn->srclo=midhi+1;
  btn->srchi=INT_MAX;
  btn->flags=0;
  btn->dstbtnid=(dstbtnid&(GAMEK_BUTTON_RIGHT|GAMEK_BUTTON_DOWN));
  
  ctx->buttons|=dstbtnid;
  if (dstbtnid==GAMEK_BUTTON_HORZ) {
    ctx->count_by_btnid[GAMEK_BTNIX_LEFT]++;
    ctx->count_by_btnid[GAMEK_BTNIX_RIGHT]++;
  } else {
    ctx->count_by_btnid[GAMEK_BTNIX_UP]++;
    ctx->count_by_btnid[GAMEK_BTNIX_DOWN]++;
  }
  
  return 0;
}

/* Add a rotational hat.
 */
 
static int gamek_inmgr_tm_add_hat(
  struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_capability *cap,
  struct gamek_inmgr_tmsyn_context *ctx
) {
  struct gamek_inmgr_tm_btn *btn=gamek_inmgr_tm_btnv_append(tm);
  if (!btn) return -1;
  btn->srcbtnid=cap->btnid;
  btn->srclo=cap->lo;
  btn->srchi=cap->hi;
  btn->flags=0;
  btn->dstbtnid=GAMEK_BUTTON_HAT;
  ctx->buttons|=GAMEK_BUTTON_HAT;
  ctx->count_by_btnid[GAMEK_BTNIX_LEFT]++;
  ctx->count_by_btnid[GAMEK_BTNIX_RIGHT]++;
  ctx->count_by_btnid[GAMEK_BTNIX_UP]++;
  ctx->count_by_btnid[GAMEK_BTNIX_DOWN]++;
  return 0;
}

/* Add a button or axis with output unspecified.
 */
 
static int gamek_inmgr_tm_add_button_any(
  struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_capability *cap,
  struct gamek_inmgr_tmsyn_context *ctx
) {
  const uint16_t mappable=
    GAMEK_BUTTON_SOUTH|
    GAMEK_BUTTON_WEST|
    GAMEK_BUTTON_EAST|
    GAMEK_BUTTON_NORTH|
    GAMEK_BUTTON_L1|
    GAMEK_BUTTON_R1|
    GAMEK_BUTTON_L2|
    GAMEK_BUTTON_R2|
    GAMEK_BUTTON_AUX1|
    GAMEK_BUTTON_AUX2|
    GAMEK_BUTTON_AUX3|
  0;
  int dstbtnid=0,loc=INT_MAX,mask=1,p=0;
  for (;mask<0x10000;mask<<=1,p++) {
    if (!(mask&mappable)) continue;
    if (ctx->count_by_btnid[p]<loc) {
      dstbtnid=mask;
      loc=ctx->count_by_btnid[p];
    }
  }
  if (!dstbtnid) return 0;
  return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,dstbtnid);
}
 
static int gamek_inmgr_tm_add_axis_any(
  struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_capability *cap,
  struct gamek_inmgr_tmsyn_context *ctx
) {
  int xc=ctx->count_by_btnid[GAMEK_BTNIX_LEFT];
  if (ctx->count_by_btnid[GAMEK_BTNIX_RIGHT]<xc) xc=ctx->count_by_btnid[GAMEK_BTNIX_RIGHT];
  int yc=ctx->count_by_btnid[GAMEK_BTNIX_UP];
  if (ctx->count_by_btnid[GAMEK_BTNIX_DOWN]<yc) yc=ctx->count_by_btnid[GAMEK_BTNIX_DOWN];
  int dstbtnid=(xc<=yc)?GAMEK_BUTTON_HORZ:GAMEK_BUTTON_VERT;
  return gamek_inmgr_tm_add_axis(tm,cap,ctx,dstbtnid);
}

/* Consider one device capability, and maybe add a template mapping for it.
 */
 
static int gamek_inmgr_tm_add_cap(
  struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_capability *cap,
  struct gamek_inmgr_tmsyn_context *ctx
) {
  
  // Hard-coded opinions for some obvious 2-state buttons. eg keyboards.
  if ((cap->lo==0)&&((cap->hi==1)||(cap->hi==2))) switch (cap->usage) {
    case 0x0001008a: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_RIGHT);
    case 0x0001008b: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_LEFT);
    case 0x0001008c: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_UP);
    case 0x0001008d: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_DOWN);
    case 0x00010090: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_UP);
    case 0x00010091: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_DOWN);
    case 0x00010092: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_RIGHT);
    case 0x00010093: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_LEFT);
    case 0x00070004: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_EAST); // a
    case 0x00070016: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_NORTH); // s
    case 0x0007001b: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_WEST); // x
    case 0x0007001d: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_SOUTH); // z
    case 0x0007001e: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_L2); // 1
    case 0x00070028: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_AUX1); // enter
    case 0x00070029: return gamek_inmgr_tm_add_action(tm,cap->btnid,ctx,GAMEK_ACTION_QUIT); // ESC
    case 0x0007002a: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_R1); // backspace
    case 0x0007002b: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_AUX2); // tab
    case 0x0007002c: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_AUX3); // space
    case 0x0007002e: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_R2); // equal
    case 0x00070035: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_L1); // grave
    case 0x00070044: return gamek_inmgr_tm_add_action(tm,cap->btnid,ctx,GAMEK_ACTION_FULLSCREEN); // F11
    case 0x0007004f: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_RIGHT); // arrow
    case 0x00070050: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_LEFT); // arrow
    case 0x00070051: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_DOWN); // arrow
    case 0x00070052: return gamek_inmgr_tm_add_twostate(tm,cap->btnid,ctx,GAMEK_BUTTON_UP); // arrow
  }
  
  // Axes identifiable by usage.
  if (cap->lo<=cap->hi-2) switch (cap->usage) {
    case 0x00010030: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00010031: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
    case 0x00010033: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00010034: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
    case 0x00010040: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00010041: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
    case 0x00010043: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00010044: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
    case 0x00050021: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00050022: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
    case 0x00050023: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00050024: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00050025: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
    case 0x00050026: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
    case 0x00050027: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_HORZ);
    case 0x00050028: return gamek_inmgr_tm_add_axis(tm,cap,ctx,GAMEK_BUTTON_VERT);
  }
  
  // Hat switch.
  if ((cap->lo<=cap->hi-7)&&(cap->usage==0x00010039)) {
    return gamek_inmgr_tm_add_hat(tm,cap,ctx);
  }
  
  // Didn't recognize the usage.
  // Take a wild guess based on the range.
  if ((cap->lo<cap->value)&&(cap->value<cap->hi)) return gamek_inmgr_tm_add_axis_any(tm,cap,ctx);
  if ((cap->usage<0x00070000)||(cap->usage>0x0007ffff)) {
    // ^ No "any button" for keyboards.
    if ((cap->lo==0)&&(cap->hi>0)) return gamek_inmgr_tm_add_button_any(tm,cap,ctx);
  }
  
  // Don't guess anything else, just leave it unmapped.
  
  return 0;
}

/* Synthesize template, context provided.
 */
 
static int gamek_inmgr_synthesize_template_inner(
  struct gamek_inmgr_tm *tm,
  const struct gamek_inmgr_joystick_greeting *greeting,
  struct gamek_inmgr *inmgr
) {

  struct gamek_inmgr_tmsyn_context ctx={0};

  const struct gamek_inmgr_joystick_capability *cap=greeting->capv;
  int i=greeting->capc;
  for (;i-->0;cap++) {
    if (gamek_inmgr_tm_add_cap(tm,cap,&ctx)<0) return -1;
  }
  if (!gamek_inmgr_tmsyn_context_valid(&ctx)) return -1;

  // Normalize name before committing it: Lowercase, all punctuation becomes space, trim head and tail.
  if (greeting->name&&greeting->name[0]) {
    const char *src=greeting->name;
    int srcc=0; while (src[srcc]) srcc++;
    int leadc=0;
    while (leadc<srcc) {
      if ((src[leadc]>='A')&&(src[leadc]<='Z')) break;
      if ((src[leadc]>='a')&&(src[leadc]<='z')) break;
      if ((src[leadc]>='0')&&(src[leadc]<='9')) break;
      leadc++;
    }
    while (leadc<srcc) {
      if ((src[srcc-1]>='A')&&(src[srcc-1]<='Z')) break;
      if ((src[srcc-1]>='a')&&(src[srcc-1]<='z')) break;
      if ((src[srcc-1]>='0')&&(src[srcc-1]<='9')) break;
      srcc--;
    }
    if (leadc<srcc) {
      src+=leadc;
      srcc-=leadc;
      if (!(tm->name=malloc(srcc+1))) return -1;
      memcpy(tm->name,src,srcc);
      tm->name[srcc]=0;
      i=srcc; while (i-->0) {
             if ((tm->name[i]>='A')&&(tm->name[i]<='Z')) tm->name[i]+=0x20;
        else if ((tm->name[i]>='a')&&(tm->name[i]<='z')) ;
        else if ((tm->name[i]>='0')&&(tm->name[i]<='9')) ;
        else tm->name[i]=' ';
      }
    }
  }
  
  tm->vid=greeting->vid;
  tm->pid=greeting->pid;

  return 0;
}

/* Synthesize template from greeting.
 */
 
struct gamek_inmgr_tm *gamek_inmgr_synthesize_template(
  struct gamek_inmgr *inmgr,
  const struct gamek_inmgr_joystick_greeting *greeting
) {
  struct gamek_inmgr_tm *tm=gamek_inmgr_tm_add(inmgr);
  if (!tm) return 0;
  if (gamek_inmgr_synthesize_template_inner(tm,greeting,inmgr)<0) {
    gamek_inmgr_tm_remove(inmgr,tm);
    return 0;
  }
  return tm;
}

/* Apply template.
 */

int gamek_inmgr_map_device(
  struct gamek_inmgr *inmgr,
  const struct gamek_inmgr_joystick_greeting *greeting,
  const struct gamek_inmgr_tm *tm
) {
  if (!inmgr||!greeting||!tm) return -1;
  
  uint8_t pid1=0,pid2=0;
  int advance_playerid_next=0;
  
  const struct gamek_inmgr_tm_btn *btn=tm->btnv;
  int btni=tm->btnc;
  for (;btni-->0;btn++) {
  
    struct gamek_inmgr_joystick_capability *cap;
    if (btn->flags&GAMEK_INMGR_TM_BTN_FLAG_USAGE) {
      cap=gamek_inmgr_joystick_greeting_get_capability_by_usage(greeting,btn->srcbtnid);
    } else {
      cap=gamek_inmgr_joystick_greeting_get_capability(greeting,btn->srcbtnid);
    }
    if (!cap) continue; // Button in template but not on device. That's OK.
    
    int mapp=gamek_inmgr_mapv_search(inmgr,greeting->id,btn->srcbtnid);
    if (mapp<0) mapp=-mapp-1;
    struct gamek_inmgr_map *map=gamek_inmgr_mapv_insert(inmgr,mapp,greeting->id,btn->srcbtnid,1);
    if (!map) return -1;
    
    // There are three different things: button, hat, action.
    // But for the most part we don't need to distinguish them here.
    map->srclo=btn->srclo;
    map->srchi=btn->srchi;
    map->dstbtnid=btn->dstbtnid;
    
    if (btn->flags&GAMEK_INMGR_TM_BTN_FLAG_ACTION) {
      map->playerid=0; // action indicator
    } else if (btn->flags&GAMEK_INMGR_TM_BTN_FLAG_PLAYER2) {
      if (!pid2) {
        if (!pid1) pid1=inmgr->playerid_next;
        pid2=inmgr->playerid_next+1;
        if (pid2>inmgr->playerid_max) pid2=1;
      }
      map->playerid=pid2;
      if (!(btn->flags&GAMEK_INMGR_TM_BTN_FLAG_LOWPRIORITY)) advance_playerid_next=1;
    } else {
      if (!pid1) pid1=inmgr->playerid_next;
      map->playerid=pid1;
      if (!(btn->flags&GAMEK_INMGR_TM_BTN_FLAG_LOWPRIORITY)) advance_playerid_next=1;
    }
  }
  
  if (advance_playerid_next&&pid1) {
    if (inmgr->playerid_next>=inmgr->playerid_max) inmgr->playerid_next=1;
    else inmgr->playerid_next++;
    if (pid2) {
      if (inmgr->playerid_next>=inmgr->playerid_max) inmgr->playerid_next=1;
      else inmgr->playerid_next++;
    }
  }
  if (pid1) gamek_inmgr_send_button(inmgr,pid1,GAMEK_BUTTON_CD,1);
  if (pid2) gamek_inmgr_send_button(inmgr,pid2,GAMEK_BUTTON_CD,1);
  return 0;
}
