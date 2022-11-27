#ifndef OSSMIDI_INTERNAL_H
#define OSSMIDI_INTERNAL_H

#include "ossmidi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/inotify.h>

struct ossmidi_device {
  int devid;
  int fd;
};

struct ossmidi {
  struct ossmidi_delegate delegate;
  int infd;
  int rescan;
  struct ossmidi_device *devicev;
  int devicec,devicea;
  struct pollfd *pollfdv;
  int pollfdc,pollfda;
};

#endif
