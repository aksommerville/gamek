#include "gamek_inmgr_internal.h"
#include "opt/fs/gamek_fs.h"

/* Encode and save.
 */
 
int gamek_inmgr_save_configuration(struct gamek_inmgr *inmgr,const char *path) {
  char *src=0;
  int srcc=gamek_inmgr_encode_configuration(&src,inmgr);
  if (srcc<0) return -1;
  int err=gamek_file_write(path,src,srcc);
  free(src);
  return err;
}

/* Configure from a file.
 */
 
int gamek_inmgr_configure_file(struct gamek_inmgr *inmgr,const char *path) {
  char *src=0;
  int srcc=gamek_file_read(&src,path);
  if (srcc<0) return -1;
  int err=gamek_inmgr_configure_text(inmgr,src,srcc,path);
  free(src);
  return err;
}

/* Simulate a device connection for the system keyboard.
 */
 
static int gamek_inmgr_connect_keyboard(struct gamek_inmgr *inmgr) {
  struct gamek_inmgr_joystick_greeting greeting={
    .id=0,
    .name="System keyboard",
    .vid=0,
    .pid=0,
  };
  
  // Keyboard button IDs are expected to be USB-HID usage codes.
  int btnid;
  #define BUTTONS(lo,hi) { \
    for (btnid=0x00070000+lo;btnid<=0x00070000+hi;btnid++) { \
      if (gamek_inmgr_joystick_greeting_add_capability( \
        &greeting,btnid,btnid,0,2,0 \
      )<0) { \
        gamek_inmgr_joystick_greeting_cleanup(&greeting); \
        return -1; \
      } \
    } \
  }
  BUTTONS(0x04,0x86) // Most keyboard and keypad buttons, and a broad set of oddballs too.
  BUTTONS(0xe0,0xe7) // Modifier keys.
  #undef BUTTONS
  
  int err=gamek_inmgr_connect_private(inmgr,&greeting);
  gamek_inmgr_joystick_greeting_cleanup(&greeting);
  return err;
}

/* Finalize configuration.
 */
 
int gamek_inmgr_ready(struct gamek_inmgr *inmgr) {
  if (!inmgr) return -1;
  if (inmgr->ready) return 0;
  inmgr->ready=1;
  
  if (inmgr->delegate.has_keyboard) {
    if (gamek_inmgr_connect_keyboard(inmgr)<0) return -1;
  }
  
  return 0;
}
