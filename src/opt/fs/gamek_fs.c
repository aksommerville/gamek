#include "gamek_fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <string.h>

#ifndef O_BINARY
  #define O_BINARY 0
#endif

/* Read file.
 */
 
int gamek_file_read(void *dstpp,const char *path) {
  if (!dstpp||!path) return -1;
  int fd=open(path,O_RDONLY|O_BINARY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=malloc(flen?flen:1);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Write file.
 */
 
int gamek_file_write(const char *path,const void *src,int srcc) {
  if (!path||(srcc<0)||(srcc&&!src)) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
  if (fd<0) return -1;
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}

/* Make directory and parents.
 */
 
int gamek_mkdirp(const char *path) {
  if (!path) return -1;
  if (mkdir(path,0775)>=0) return 0;
  if (errno==EEXIST) return 0;
  if (errno!=ENOENT) return -1; 
  int slashp=-1,i=0;
  for (;path[i];i++) {
    if (path[i]=='/') slashp=i;
  }
  if (slashp<1) return -1;
  char parentpath[1024];
  if (slashp>=sizeof(parentpath)) return -1;
  memcpy(parentpath,path,slashp);
  parentpath[slashp]=0;
  if (gamek_mkdirp(parentpath)<0) return -1;
  if (mkdir(path,0775)<0) return -1;
  return 0;
}

/* Last separator.
 */
 
int gamek_path_find_separator(const char *path,int pathc) {
  if (!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  int p=pathc;
  // Ignore trailing separators.
  while (p&&((path[p-1]=='/')||(path[p-1]=='\\'))) p--;
  while (p&&(path[p-1]!='/')&&(path[p-1]!='\\')) p--;
  return p-1;
}
