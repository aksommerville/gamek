/* akdrm.h
 * Direct Rendering Manager for Linux. Video without an X server.
 * Like akx11, you can initialize with GX, GX-backed framebuffer, or plain framebuffer.
 * We do not offer as wide a selection of framebuffer formats, due only to laziness.
 */
 
#ifndef AKDRM_H
#define AKDRM_H

struct akdrm;

#define AKDRM_VIDEO_MODE_AUTO    0
#define AKDRM_VIDEO_MODE_OPENGL  1 /* Expose an OpenGL context. */
#define AKDRM_VIDEO_MODE_FB_PURE 2 /* Expose a framebuffer and don't touch OpenGL. (scale up in software) */
#define AKDRM_VIDEO_MODE_FB_GX   3 /* Expose a framebuffer but use OpenGL to scale up. */

#define AKDRM_FBFMT_ANYTHING   0 /* Will normally be some 32-bit RGB variation. */
#define AKDRM_FBFMT_XRGB       1 /* The RGB formats are named big-endianly as if you read them as a 32-bit integer. */
#define AKDRM_FBFMT_XBGR       2 /* So on a little-endian host, XBGR means R is the first byte serially. */
#define AKDRM_FBFMT_RGBX       3
#define AKDRM_FBFMT_BGRX       4
#define AKDRM_FBFMT_Y8         5

/* Selecting a device and mode.
 ***************************************************************************/

/* Iterate video configurations.
 * There will be more than one per card, and possibly more than one card.
 */
struct akdrm_config {
  char *path; // eg "/dev/dri/card0"
  int rate; // hz
  int w,h; // pixels
  int current; // nonzero if this mode is active already
  int connector_id;
  int mode_index;
  char *mode_name;
  int flags; // DRM_MODE_FLAGS_* from Linux drm_mode.h
  int type; // DRM_MODE_TYPE_* from Linux drm_mode.h
};
int akdrm_list_devices(
  const char *path, // null for default "/dev/dri"
  int (*cb)(struct akdrm_config *config,void *userdata),
  void *userdata
);

/* Populate (config) with our opinion of the best match.
 * Caller must clean up (config) after a success.
 */
int akdrm_find_device(
  struct akdrm_config *config,
  const char *path, // usually null
  int rate, // preferred refresh rate in hz, or 0
  int w,int h, // preferred absolute size in pixels, or 0
  int fbw,int fbh // size of your framebuffer, to guide aspect comparison, or 0
);

void akdrm_config_cleanup(struct akdrm_config *config);

/* Context.
 ***********************************************************************/

/* AKDRM_VIDEO_MODE_OPENGL is the simplest.
 *   You get an OpenGL context, and we really don't touch it.
 * AKDRM_VIDEO_MODE_FB_GX, we create an OpenGL context but you never see it.
 *   You will get a framebuffer of the exact size and format you request.
 * AKDRM_VIDEO_MODE_FB_PURE, we hand off the actual framebuffer to you.
 *   The framebuffer will generally *not* be the size you asked for; scaling and conversion is your problem.
 *   TODO: That's inconsistent with akx11's FB_PURE mode. Can we take on scaling and conversion here?
 *
 * Our setup struct doesn't have anything about device matching.
 * In DRM, that's complicated enough to get its own thing, struct akdrm_config.
 * "config" is a device and mode.
 */ 
struct akdrm_setup {
  int video_mode;
  int fbfmt;
  int fbw,fbh;
};
 
void akdrm_del(struct akdrm *akdrm);

struct akdrm *akdrm_new(
  const struct akdrm_config *config,
  const struct akdrm_setup *setup
);

int akdrm_get_video_mode(const struct akdrm *akdrm);
int akdrm_get_fbfmt(const struct akdrm *akdrm);
void akdrm_get_fb_size(int *w,int *h,const struct akdrm *akdrm);
void akdrm_get_size(int *w,int *h,const struct akdrm *akdrm);
int akdrm_begin_gx(struct akdrm *akdrm);
void akdrm_end_gx(struct akdrm *akdrm);
void *akdrm_begin_fb(struct akdrm *akdrm);
void akdrm_end_fb(struct akdrm *akdrm,void *fb);
int akdrm_fbfmt_measure_stride(int fbfmt,int w);
int akdrm_fbfmt_measure_buffer(int fbfmt,int w,int h);
int akdrm_video_mode_is_fb(int video_mode);
int akdrm_video_mode_is_gx(int video_mode);

#endif
