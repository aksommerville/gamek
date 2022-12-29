#include "macioc_internal.h"
#include <fcntl.h>
#include <unistd.h>

struct macioc macioc={0};

/* Reopen TTY.
 * Necessary for Mac GUI apps if you launch from a terminal and want logging to it.
 */

static void macioc_reopen_tty(const char *path) {
  int fd=open(path,O_RDWR);
  if (fd<0) return;
  dup2(fd,STDOUT_FILENO);
  dup2(fd,STDIN_FILENO);
  dup2(fd,STDERR_FILENO);
  close(fd);
}

/* Change working directory.
 * Mac GUI apps get their working directory sandboxed by default.
 * We want it to be the project directory, when launched from here.
 */

static void macioc_chdir(const char *path) {
  chdir(path);
}

/* Main.
 */
 
int macioc_main(int argc,char **argv,const struct macioc_delegate *delegate) {
  if (delegate) macioc.delegate=*delegate;

  int i=1; for (;i<argc;i++) {
    if (!memcmp(argv[i],"--reopen-tty=",13)) {
      macioc_reopen_tty(argv[i]+13);
      argv[i]="";
    } else if (!memcmp(argv[i],"--chdir=",8)) {
      macioc_chdir(argv[i]+8);
      argv[i]="";
    }
  }

  return NSApplicationMain(argc,(void*)argv);
}

/* Terminate.
 */

void macioc_terminate(int status) {
  //fprintf(stderr,"%s(%d)\n",__func__,status);
  macioc.terminate=1;
  [NSApplication.sharedApplication terminate:0];
}
