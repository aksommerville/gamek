#include "gamek_map.h"

/* Decode.
 */
 
int8_t gamek_map_decode(struct gamek_map *map,const void *src) {
  // This is easier than it sounds...
  if (!map||!src) return -1;
  const uint8_t *SRC=src;
  map->w=SRC[0];
  map->h=SRC[1];
  map->v=SRC+2;
  map->cmdv=SRC+2+map->w*map->h;
  return 0;
}
