/* gamek_map.h
 * A loose abstraction of scenes as a grid of tiles and arbitrary commands.
 * See etc/doc/map-format.md
 */
 
#ifndef GAMEK_MAP_H
#define GAMEK_MAP_H

#include <stdint.h>

struct gamek_map {
  uint8_t w,h;
  const uint8_t *v; // size=w*h, LRTB
  const uint8_t *cmdv; // self-terminating
};

/* Maps are self-terminated.
 * You must provide something valid (and don't forget the commands-section terminator!).
 * You don't provide the length because we assume it's a trusted source, and we aren't going to read to the end anyway.
 */
int8_t gamek_map_decode(struct gamek_map *map,const void *src);

#endif
