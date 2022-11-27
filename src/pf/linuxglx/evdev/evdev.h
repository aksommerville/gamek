/* evdev.h
 * Minimal interface to Linux evdev, for joysticks and such.
 */
 
#ifndef EVDEV_H
#define EVDEV_H

struct evdev;
struct evdev_device;

struct evdev_delegate {
  void *userdata;
  
  /* Device has become available.
   * You should assign its devid, set yourself up for mapping, and list its fd if you're polling externally.
   * Device addresses are stable, if you want to use those for identification.
   */
  void (*connect)(struct evdev_device *device);
  
  /* Device is being dropped.
   * You will not get these events at context teardown.
   */
  void (*disconnect)(struct evdev_device *device);
  
  /* Event on one device.
   * (type,code) are from <linux/input-event-codes.h>, but you can usually treat them as opaque nonces.
   */
  void (*button)(struct evdev_device *device,int type,int code,int value);
  
  /* Our inotify file has closed (unlikely).
   * You need to know this if polling externally.
   * And in all cases, this means you need to scan manually to detect new connections.
   * When we call this, evdev_get_inotify_fd() will still return the defunct fd.
   */
  void (*lost_inotify)(struct evdev *evdev);
};

struct evdev_setup {
  const char *path; // "/dev/input" if you don't specify
  int use_inotify; // If zero we won't automatically detect connections. (why would you want that?)
  int grab_devices; // Normally 1. Should we attempt EVIOCGRAB on connect.
  int guess_hid; // Normally 1, to make up HID usage codes based on Linux type/code where possible.
};

// Since the defaults are not zero, it's wise to start with this:
void evdev_setup_default(struct evdev_setup *setup);

/* Driver context.
 ***********************************************************************/

void evdev_del(struct evdev *evdev);

struct evdev *evdev_new(
  const struct evdev_delegate *delegate,
  const struct evdev_setup *setup
);

void *evdev_get_userdata(const struct evdev *evdev);

/* Use plain update and we invoke poll() ourselves, that's usually what you want.
 * You can use this API with an external poller too: evdev_update_fd() when one of my files becomes readable.
 * (You must get the inotify fd at startup, and update your fd list on connect and disconnect events).
 * update_fd returns 0 if unknown, >0 if processed, or <0 for unlikely errors.
 * In general, failure to read a file is not reported as an error.
 */
int evdev_update(struct evdev *evdev,int toms);
int evdev_update_fd(struct evdev *evdev,int fd);

/* Request to manually scan the device directory at next update.
 * This does not perform any IO immediately.
 * Deferred scanning is only available when you use full update().
 * With an external poller, call evdev_scan_now() at startup, and whenever you need it later.
 */
void evdev_scan(struct evdev *evdev);
int evdev_scan_now(struct evdev *evdev);

// <0 if inotify is defunct. In which case you might want to scan periodically.
int evdev_get_inotify_fd(const struct evdev *evdev);

int evdev_count_devices(const struct evdev *evdev);
struct evdev_device *evdev_device_by_index(const struct evdev *evdev,int p);
struct evdev_device *evdev_device_by_fd(const struct evdev *evdev,int fd);
struct evdev_device *evdev_device_by_kid(const struct evdev *evdev,int kid);
struct evdev_device *evdev_device_by_devid(const struct evdev *evdev,int devid);

// At any time, use this to report all active files.
int evdev_for_each_fd(
  const struct evdev *evdev,
  int (*cb)(int fd,void *userdata),
  void *userdata
);

/* Device.
 ***********************************************************************/
 
struct evdev *evdev_device_get_context(const struct evdev_device *device);
void *evdev_device_get_userdata(const struct evdev_device *device); // context's userdata, just for convenience.
int evdev_device_get_fd(const struct evdev_device *device);
int evdev_device_get_kid(const struct evdev_device *device); // eg 3 for /dev/input/event3
int evdev_device_get_devid(const struct evdev_device *device); // yours to assign; zero by default
int evdev_device_set_devid(struct evdev_device *device,int devid);

/* Name is fetched from the kernel each time you ask for it.
 */
int evdev_device_get_name(char *dst,int dsta,const struct evdev_device *device);

// Hardware IDs are fetched the first time you ask for one, then we cache them on the device.
int evdev_device_get_vid(struct evdev_device *device);
int evdev_device_get_pid(struct evdev_device *device);
int evdev_device_get_version(struct evdev_device *device);
int evdev_device_get_bustype(struct evdev_device *device);

/* List all device capabilities. This does perform IO.
 * (usage) is our best guess at HID usage. It is not supplied by the kernel.
 * Internally, (type) is 8 bits and (code) is 16. So they can safely be combined like ((type<<16)|code).
 */
int evdev_device_enumerate(
  struct evdev_device *device,
  int (*cb)(int type,int code,int usage,int lo,int hi,int value,void *userdata),
  void *userdata
);

/* Odds, ends.
 ***************************************************************************/

int evdev_guess_hid_usage(int type,int code);

#endif
