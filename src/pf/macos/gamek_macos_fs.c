#include "gamek_macos_internal.h"
#include "opt/fs/gamek_fs.h"
#include <fcntl.h>
#include <unistd.h>

/* Set the default sandbox path.
 */
 
static int gamek_macos_set_sandbox(const char *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  if (gamek_macos.fs_sandbox) free(gamek_macos.fs_sandbox);
  gamek_macos.fs_sandbox=nv;
  return 0;
}
 
static int gamek_macos_set_default_sandbox() {
  if (gamek_macos.fs_sandbox) return -1;

  /*XXX This made sense for linux, but maybe not for Mac? We would want to go adjacent to the app bundle (the ".app" directory).
  // If exename has a slash, use its directory part.
  // ie prefer to put data adjacent to the executable.
  int slashp=-1,i=0;
  for (;gamek_macos.exename[i];i++) {
    if (gamek_macos.exename[i]=='/') slashp=i;
  }
  if (slashp>0) return gamek_macos_set_sandbox(gamek_macos.exename,slashp);
  /**/
  
  // ~/.gamek/${CLIENT}
  if (gamek_client.title&&gamek_client.title[0]) {
    const char *home=getenv("HOME");
    if (home&&home[0]) {
      char tmp[1024];
      int tmpc=snprintf(tmp,sizeof(tmp),"%s/.gamek/%s",home,gamek_client.title);
      if ((tmpc>0)&&(tmpc<sizeof(tmp))) return gamek_macos_set_sandbox(tmp,tmpc);
    }
  }
  
  // Anything else? Working directory? Static location like /usr/local/share/gamek/${CLIENT}?
  // There's probably some conventional MacOS location for volatile app data.
  
  return -1;
}

/* Combine and validate path in sandbox.
 * Fails if too long or if it escapes the sandbox.
 */
 
static int gamek_macos_get_sandbox_path(char *dst,int dsta,const char *src) {
  if (!gamek_macos.fs_sandbox) {
    if (gamek_macos_set_default_sandbox()<0) return -1;
    if (gamek_mkdirp(gamek_macos.fs_sandbox)<0) return -1;
  }
  if (!src) return -1;
  if (!dst||(dsta<1)) return -1;
  
  // Confirm there's no double-dots.
  // I'm going to do this cheap, and forbid even double-dots in the middle of a base name.
  const char *v=src; for (;*v;v++) {
    if ((v[0]=='.')&&(v[1]=='.')) return -1;
  }
  
  int dstc=snprintf(dst,dsta,"%s/%s",gamek_macos.fs_sandbox,src);
  if (dstc<1) return -1;
  if (dstc>=dsta) return -1;
  return dstc;
}

/* Read file.
 */

int32_t gamek_platform_file_read(void *dst,int32_t dsta,const char *clientpath,int32_t seek) {
  if (!dst||(dsta<1)) return 0;
  char path[1024];
  if (gamek_macos_get_sandbox_path(path,sizeof(path),clientpath)<0) return -1;
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  if (seek&&(lseek(fd,seek,SEEK_SET)!=seek)) {
    close(fd);
    return -1;
  }
  int dstc=read(fd,dst,dsta);
  close(fd);
  return dstc;
}

/* Write entire file.
 */

int32_t gamek_platform_file_write_all(const char *clientpath,const void *src,int32_t srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  char path[1024];
  if (gamek_macos_get_sandbox_path(path,sizeof(path),clientpath)<0) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
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
  return srcc;
}

/* Write partial file.
 */

int32_t gamek_platform_file_write_part(const char *clientpath,int32_t seek,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  char path[1024];
  if (gamek_macos_get_sandbox_path(path,sizeof(path),clientpath)<0) return -1;
  int fd=open(path,O_WRONLY|O_CREAT,0666);
  if (fd<0) return -1;
  
  if (seek<0) {
    if (lseek(fd,0,SEEK_END)<0) {
      close(fd);
      return -1;
    }
  } else {
    int fp=lseek(fd,seek,SEEK_SET);
    while (fp<seek) {
      const char *zeroes="\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
      int wrc=seek-fp;
      if (wrc>16) wrc=16;
      int err=write(fd,zeroes,wrc);
      if (err<=0) {
        close(fd);
        return -1;
      }
      fp+=wrc;
    }
  }
  
  int err=write(fd,src,srcc);
  close(fd);
  return err;
}
