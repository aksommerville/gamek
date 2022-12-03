#ifndef GAMEK_GENERIC_INTERNAL_H
#define GAMEK_GENERIC_INTERNAL_H

#include "pf/gamek_pf.h"
#include "common/gamek_image.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

extern struct gamek_generic {
  volatile int terminate;
  struct gamek_image fb;
} gamek_generic;

#endif
