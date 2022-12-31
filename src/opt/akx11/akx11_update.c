#include "akx11_internal.h"

/* Key press, release, or repeat.
 */
 
static int akx11_evt_key(struct akx11 *akx11,XKeyEvent *evt,int value) {

  /* Pass the raw keystroke. */
  if (akx11->delegate.key) {
    KeySym keysym=XkbKeycodeToKeysym(akx11->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=akx11_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        int err=akx11->delegate.key(akx11->delegate.userdata,keycode,value);
        if (err) return err; // Stop here if acknowledged.
      }
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  if (akx11->delegate.text) {
    int shift=(evt->state&ShiftMask)?1:0;
    KeySym tkeysym=XkbKeycodeToKeysym(akx11->dpy,evt->keycode,0,shift);
    if (shift&&!tkeysym) { // If pressing shift makes this key "not a key anymore", fuck that and pretend shift is off
      tkeysym=XkbKeycodeToKeysym(akx11->dpy,evt->keycode,0,0);
    }
    if (tkeysym) {
      int codepoint=akx11_codepoint_from_keysym(tkeysym);
      if (codepoint && (evt->type == KeyPress || evt->type == KeyRepeat)) {
        akx11->delegate.text(akx11->delegate.userdata,codepoint);
      }
    }
  }
  
  return 0;
}

/* Mouse events.
 */
 
static int akx11_evt_mbtn(struct akx11 *akx11,XButtonEvent *evt,int value) {
  
  // I swear X11 used to automatically report the wheel as (6,7) while shift held, and (4,5) otherwise.
  // After switching to GNOME 3, seems it is only ever (4,5).
  #define SHIFTABLE(v) (evt->state&ShiftMask)?v:0,(evt->state&ShiftMask)?0:v
  
  switch (evt->button) {
    case 1: if (akx11->delegate.mbutton) akx11->delegate.mbutton(akx11->delegate.userdata,1,value); break;
    case 2: if (akx11->delegate.mbutton) akx11->delegate.mbutton(akx11->delegate.userdata,3,value); break;
    case 3: if (akx11->delegate.mbutton) akx11->delegate.mbutton(akx11->delegate.userdata,2,value); break;
    case 4: if (value&&akx11->delegate.mwheel) akx11->delegate.mwheel(akx11->delegate.userdata,SHIFTABLE(-1)); break;
    case 5: if (value&&akx11->delegate.mwheel) akx11->delegate.mwheel(akx11->delegate.userdata,SHIFTABLE(1)); break;
    case 6: if (value&&akx11->delegate.mwheel) akx11->delegate.mwheel(akx11->delegate.userdata,-1,0); break;
    case 7: if (value&&akx11->delegate.mwheel) akx11->delegate.mwheel(akx11->delegate.userdata,1,0); break;
  }
  #undef SHIFTABLE
  return 0;
}

static int akx11_evt_mmotion(struct akx11 *akx11,XMotionEvent *evt) {
  if (akx11->delegate.mmotion) {
    akx11->delegate.mmotion(akx11->delegate.userdata,evt->x,evt->y);
  }
  return 0;
}

/* Client message.
 */
 
static int akx11_evt_client(struct akx11 *akx11,XClientMessageEvent *evt) {
  if (evt->message_type==akx11->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==akx11->atom_WM_DELETE_WINDOW) {
        if (akx11->delegate.close) {
          akx11->delegate.close(akx11->delegate.userdata);
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int akx11_evt_configure(struct akx11 *akx11,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=akx11->w)||(nh!=akx11->h)) {
    akx11->dstdirty=1;
    akx11->w=nw;
    akx11->h=nh;
    akx11_reconsider_scaling_filter(akx11);
    if (akx11->delegate.resize) {
      akx11->delegate.resize(akx11->delegate.userdata,nw,nh);
    }
  }
  return 0;
}

/* Focus.
 */
 
static int akx11_evt_focus(struct akx11 *akx11,XFocusInEvent *evt,int value) {
  if (value==akx11->focus) return 0;
  akx11->focus=value;
  if (akx11->delegate.focus) {
    akx11->delegate.focus(akx11->delegate.userdata,value);
  }
  return 0;
}

/* Process one event.
 */
 
static int akx11_receive_event(struct akx11 *akx11,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return akx11_evt_key(akx11,&evt->xkey,1);
    case KeyRelease: return akx11_evt_key(akx11,&evt->xkey,0);
    case KeyRepeat: return akx11_evt_key(akx11,&evt->xkey,2);
    
    case ButtonPress: return akx11_evt_mbtn(akx11,&evt->xbutton,1);
    case ButtonRelease: return akx11_evt_mbtn(akx11,&evt->xbutton,0);
    case MotionNotify: return akx11_evt_mmotion(akx11,&evt->xmotion);
    
    case ClientMessage: return akx11_evt_client(akx11,&evt->xclient);
    
    case ConfigureNotify: return akx11_evt_configure(akx11,&evt->xconfigure);
    
    case FocusIn: return akx11_evt_focus(akx11,&evt->xfocus,1);
    case FocusOut: return akx11_evt_focus(akx11,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int akx11_update(struct akx11 *akx11) {
  int evtc=XEventsQueued(akx11->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(akx11->dpy,&evt);
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(akx11->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-AKX11_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (akx11_receive_event(akx11,&evt)<0) return -1;
      } else {
        if (akx11_receive_event(akx11,&evt)<0) return -1;
        if (akx11_receive_event(akx11,&next)<0) return -1;
      }
    } else {
      if (akx11_receive_event(akx11,&evt)<0) return -1;
    }
  }
  return 0;
}
