#ifndef ALSAMIDI_INTERNAL_H
#define ALSAMIDI_INTERNAL_H

#include "alsamidi.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sound/asequencer.h>

struct alsamidi {
  struct alsamidi_delegate delegate;
  int fd;
  char *name;
  int client_id;
};

#endif
