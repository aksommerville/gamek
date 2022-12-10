#include "gamek_tiny_internal.h"
#include <stdint.h>
#include <SD.h>

//TODO Can I arrange this such that files stay open, so two requests on the same file there's no extra close and open?

/* Initialize if necessary.
 */
 
static int8_t gamek_tiny_sd_require() {
  if (!gamek_tiny.sdinit) {
    if (SD.begin()<0) return -1;
    gamek_tiny.sdinit=1;
  }
  return 0;
}

static int8_t gamek_tiny_fs_prep(char *dst,uint16_t dsta,const char *clientpath) {
  if (!gamek_tiny.fs_sandboxc) return -1;
  if (!clientpath||!clientpath[0]) return -1;
  uint16_t clientpathc=0;
  while (clientpath[clientpathc]) clientpathc++;
  uint16_t dstc=1+gamek_tiny.fs_sandboxc+1+clientpathc;
  if (dstc>=dsta) return -1;
  dst[0]='/';
  memcpy(dst+1,gamek_tiny.fs_sandbox,gamek_tiny.fs_sandboxc);
  dst[1+gamek_tiny.fs_sandboxc]='/';
  memcpy(dst+1+gamek_tiny.fs_sandboxc+1,clientpath,clientpathc);
  dst[dstc]=0;
  if (gamek_tiny_sd_require()<0) return -1;
  return 0;
}

/* Read file.
 */
 
int32_t gamek_platform_file_read(void *dst,int32_t dsta,const char *clientpath,int32_t seek) {
  if (dsta>0xffff) dsta=0xffff;
  char path[256];
  if (gamek_tiny_fs_prep(path,sizeof(path),clientpath)<0) return -1;
  File file=SD.open(path);
  if (!file) return -1;
  if (seek&&!file.seek(seek)) {
    file.close();
    return 0;
  }
  int32_t dstc=file.read(dst,dsta);
  file.close();
  return dstc;
}

/* Write file.
 */

int32_t gamek_platform_file_write_all(const char *clientpath,const void *src,int32_t srcc) {
  char path[256];
  if (gamek_tiny_fs_prep(path,sizeof(path),clientpath)<0) return -1;
  File file=SD.open(path,O_RDWR|O_CREAT|O_TRUNC);
  if (!file) return -1;
  int err=file.write((const uint8_t*)src,(size_t)srcc);
  file.close();
  return err;
}

int32_t gamek_platform_file_write_part(const char *clientpath,int32_t seek,const void *src,int srcc) {
  char path[256];
  if (gamek_tiny_fs_prep(path,sizeof(path),clientpath)<0) return -1;
  File file=SD.open(path,O_RDWR|O_CREAT);
  if (!file.seek(seek)) {
    while (file.position()<seek) {
      uint8_t tmp[32]={0};
      int err=file.write(tmp,32);
      if (err!=32) {
        file.close();
        return -1;
      }
    }
  }
  int err=file.write((const uint8_t*)src,(size_t)srcc);
  file.close();
  return err;
}
