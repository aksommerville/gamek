#include "gamek_inmgr_internal.h"

/* Delete.
 */
 
void gamek_inmgr_del(struct gamek_inmgr *inmgr) {
  if (!inmgr) return;
  
  if (inmgr->mapv) free(inmgr->mapv);
  if (inmgr->tmv) {
    while (inmgr->tmc-->0) gamek_inmgr_tm_cleanup(inmgr->tmv+inmgr->tmc);
    free(inmgr->tmv);
  }
  if (inmgr->midiv) free(inmgr->midiv);
  
  free(inmgr);
}

/* New.
 */

struct gamek_inmgr *gamek_inmgr_new(
  const struct gamek_inmgr_delegate *delegate
) {
  if (!delegate) return 0;
  struct gamek_inmgr *inmgr=calloc(1,sizeof(struct gamek_inmgr));
  if (!inmgr) return 0;
  
  inmgr->delegate=*delegate;
  inmgr->enable=1;
  inmgr->playerid_next=1;
  inmgr->playerid_max=16; // TODO expose via api
  
  return inmgr;
}

/* Search maps.
 */
 
int gamek_inmgr_mapv_search(const struct gamek_inmgr *inmgr,const void *id,int srcbtnid) {
  int lo=0,hi=inmgr->mapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct gamek_inmgr_map *map=inmgr->mapv+ck;
         if (id<map->id) hi=ck;
    else if (id>map->id) lo=ck+1;
    else if (srcbtnid<map->srcbtnid) hi=ck;
    else if (srcbtnid>map->srcbtnid) lo=ck+1;
    else {
      while ((ck>lo)&&(map[-1].id==id)&&(map[-1].srcbtnid==srcbtnid)) { ck--; map--; }
      return ck;
    }
  }
  return -lo-1;
}

/* Insert maps.
 */

struct gamek_inmgr_map *gamek_inmgr_mapv_insert(struct gamek_inmgr *inmgr,int p,const void *id,int srcbtnid,int c) {
  if ((p<0)||(p>inmgr->mapc)) return 0;
  if (p) {
    if (id<inmgr->mapv[p-1].id) return 0;
    if (id==inmgr->mapv[p-1].id) {
      if (srcbtnid<inmgr->mapv[p-1].srcbtnid) return 0; // sic not <=, dups allowed
    }
  }
  if (p<inmgr->mapc) {
    if (id>inmgr->mapv[p].id) return 0;
    if (id==inmgr->mapv[p].id) {
      if (srcbtnid>inmgr->mapv[p].srcbtnid) return 0; // sic not >=, dups allowed
    }
  }

  if (c<1) return 0;
  if (c>256) return 0; // sanity limit
  int na=inmgr->mapc+c;
  if (na>inmgr->mapa) {
    if (na<INT_MAX-256) na=(na+256)&~255;
    if (na>INT_MAX/sizeof(struct gamek_inmgr_map)) return 0;
    void *nv=realloc(inmgr->mapv,sizeof(struct gamek_inmgr_map)*na);
    if (!nv) return 0;
    inmgr->mapv=nv;
    inmgr->mapa=na;
  }
  
  struct gamek_inmgr_map *map=inmgr->mapv+p;
  memmove(map+c,map,sizeof(struct gamek_inmgr_map)*(inmgr->mapc-p));
  inmgr->mapc+=c;
  memset(map,0,sizeof(struct gamek_inmgr_map)*c);
  
  struct gamek_inmgr_map *mapp=map;
  int mapi=c;
  for (;mapi-->0;mapp++) {
    mapp->id=id;
    mapp->srcbtnid=srcbtnid;
  }
  
  return map;
}

/* Enable/disable.
 */
 
void gamek_inmgr_enable(struct gamek_inmgr *inmgr,int enable) {
  if (!inmgr) return;
  
  if (enable&&!inmgr->enable) {
    inmgr->enable=1;
    // Trigger callbacks for any current player state that doesn't match the state when we disabled.
    if (inmgr->delegate.button) {
      const int16_t *prev=inmgr->state_at_disable;
      const int16_t *next=inmgr->state_by_playerid;
      int playerid=0; for (;playerid<256;playerid++,prev++,next++) {
        if (*prev==*next) continue;
      
        // CD=1 must come first.
        if (((*next)&GAMEK_BUTTON_CD)&&!((*prev)&GAMEK_BUTTON_CD)) {
          inmgr->delegate.button(playerid,GAMEK_BUTTON_CD,1,inmgr->delegate.userdata);
        }
      
        // Order not important for other bits.
        uint16_t on=((*next)&~(*prev))&~GAMEK_BUTTON_CD;
        uint16_t off=((*prev)&!(*next))&~GAMEK_BUTTON_CD;
        if (on) {
          uint16_t bit=0x8000; for (;bit;bit>>=1) {
            if (on&bit) inmgr->delegate.button(playerid,bit,1,inmgr->delegate.userdata);
          }
        }
        if (off) {
          uint16_t bit=0x8000; for (;bit;bit>>=1) {
            if (off&bit) inmgr->delegate.button(playerid,bit,0,inmgr->delegate.userdata);
          }
        }
      
        // CD=0 must come last.
        if (!((*next)&GAMEK_BUTTON_CD)&&((*prev)&GAMEK_BUTTON_CD)) {
          inmgr->delegate.button(playerid,GAMEK_BUTTON_CD,0,inmgr->delegate.userdata);
        }
      }
    }
    
  } else if (!enable&&inmgr->enable) {
    inmgr->enable=0;
    // Record the full player state for later examination.
    memcpy(inmgr->state_at_disable,inmgr->state_by_playerid,sizeof(inmgr->state_by_playerid));
  }
}
