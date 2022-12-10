/* gamek_pf.h
 * Definition of a platform, for application's consideration.
 * There is exactly one platform, selected at build time.
 */
 
#ifndef GAMEK_PF_H
#define GAMEK_PF_H

#ifdef __cplusplus
  extern "C" {
#endif

#include <stdint.h>
#ifdef GAMEK_PLATFORM_HEADER
  #include GAMEK_PLATFORM_HEADER
#endif

struct gamek_image;

#define GAMEK_PLATFORM_CAPABILITY_TERMINATE      0x00000001 /* Has an OS or something, to terminate to. */
#define GAMEK_PLATFORM_CAPABILITY_KEYBOARD       0x00000002 /* Reasonable to assume a keyboard is present. */
#define GAMEK_PLATFORM_CAPABILITY_MOUSE          0x00000004 /* Reasonable to assume a pointing device is present. */
#define GAMEK_PLATFORM_CAPABILITY_PLUG_INPUT     0x00000008 /* Input devices might connect or disconnect on the fly. */
#define GAMEK_PLATFORM_CAPABILITY_MULTIPLAYER    0x00000010 /* Can have more than one local player, ie not a handheld. */
#define GAMEK_PLATFORM_CAPABILITY_NETWORK        0x00000020 /* Probably has a network interface. */
#define GAMEK_PLATFORM_CAPABILITY_AUDIO          0x00000040 /* Has PCM output. */
#define GAMEK_PLATFORM_CAPABILITY_MIDI           0x00000080 /* May have MIDI input. */
#define GAMEK_PLATFORM_CAPABILITY_LANGUAGE       0x00000100 /* Has OS facilities to determine user's language. */
#define GAMEK_PLATFORM_CAPABILITY_STORAGE        0x00000200 /* Probably has permanent storage, eg hard drive. */

#define GAMEK_BUTTON_LEFT     0x0001 /* Dpad... */
#define GAMEK_BUTTON_RIGHT    0x0002
#define GAMEK_BUTTON_UP       0x0004
#define GAMEK_BUTTON_DOWN     0x0008
#define GAMEK_BUTTON_SOUTH    0x0010 /* Thumb buttons... */
#define GAMEK_BUTTON_WEST     0x0020 /* If fewer than four are available, please assign in the order listed here. */
#define GAMEK_BUTTON_EAST     0x0040 /* SOUTH should be "primary" and "yes" */
#define GAMEK_BUTTON_NORTH    0x0080 /* WEST should be "secondary" and "no" */
#define GAMEK_BUTTON_L1       0x0100 /* Triggers. */
#define GAMEK_BUTTON_R1       0x0200 /* "1" should be the more reachable, ie closest to the front face. */
#define GAMEK_BUTTON_L2       0x0400
#define GAMEK_BUTTON_R2       0x0800
#define GAMEK_BUTTON_AUX1     0x1000 /* Auxiliaries. */
#define GAMEK_BUTTON_AUX2     0x2000 /* I recommend AUX1=>Start, AUX2=>Select, AUX3=>Heart, if all three are present. */
#define GAMEK_BUTTON_AUX3     0x4000 /* Deliberately vague on their meaning. Avoid using for in-game activity. */
#define GAMEK_BUTTON_CD       0x8000 /* Carrier detect. Always set, if the device is connected. */

extern const struct gamek_platform_details {
  const char *name; // For human consumption.
  uint32_t capabilities; // bits, GAMEK_PLATFORM_CAPABILITY_*
} gamek_platform_details;

/* Request termination, not necessarily immediately.
 * (status) should be zero for success or nonzero for errors.
 * Platforms that have no concept of termination must fake it.
 */
void gamek_platform_terminate(uint8_t status);

/* Send a MIDI-like event to the synthesizer.
 * I'm thinking platforms will supply synthesizers with hard-coded config.
 * You get 9 instruments and 10 PCM banks, and we'll establish some convention around chid, eg channel 0 is a violin...
 */
void gamek_platform_audio_event(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b);

/* Play song, upload audio configuration.
 * Formats and other requirements will vary depending on the platform's synthesizer.
 * (TODO Get more specific here, once the synthesizers are written).
 */
void gamek_platform_play_song(const void *v,uint16_t c);
void gamek_platform_set_audio_wave(uint8_t waveid,const int16_t *v,uint16_t c);
void gamek_platform_audio_configure(const void *v,uint16_t c);

/* Read and write files.
 * Platform must provide a filesystem sandbox, so your path should generally be like "/hiscore.dat" or maybe "/data/0001.map".
 * Writing a file, you have the option to do the whole thing at once, or a chunk, extending but not truncating.
 * (seek<0) to append.
 * Be prepared for failure; a platform might stub these out.
 */
int32_t gamek_platform_file_read(void *dst,int32_t dsta,const char *path,int32_t seek);
int32_t gamek_platform_file_write_all(const char *path,const void *src,int32_t srcc);
int32_t gamek_platform_file_write_part(const char *path,int32_t seek,const void *src,int srcc);

/* Client hooks.
 * These are not part of the platform.
 * Your program must define (gamek_client), populated with the hooks for things you can do.
 *************************************************************************/

extern const struct gamek_client {
  const char *title; // For human consumption eg logging.
  int16_t fbw,fbh; // (0,0) if you'll accept any framebuffer size. Otherwise platform will provide exactly this.
  const void *iconrgba; // Optional icon for window manager. RGBA packed LRTB.
  int iconw,iconh;

  /* Called exactly once, as soon as the platform is ready to use.
   * This will be the first call your client receives.
   */
  int8_t (*init)();
  
  /* Called repeatedly, normally at the video refresh rate (but not necessarily).
   */
  void (*update)();
  
  /* Platform is ready for a fresh frame of video.
   * Return nonzero if you write it.
   * If you return zero, it is undefined whether changes will take.
   * So do that only if you actually didn't change anything.
   */
  uint8_t (*render)(struct gamek_image *fb);
  
  /* Input state changed.
   * (btnid) will always be exactly one bit.
   * This is pretty much all you get; platforms are not required to track state for you.
   * (playerid) is zero for the aggregate of all inputs, or a small integer (eg "player one", "player two").
   * The special button "CD" is Carrier Detect, means a device is present.
   */
  void (*input_event)(uint8_t playerid,uint16_t btnid,uint8_t value);
  
  /* MIDI input, if you're doing that.
   * Platform parses the streams and presents split events instead of serial.
   * A well-behaved platform should override incoming channel assignments and use one channel per device.
   * (chid) 0..15 for regular input, 0xff for systemwide events. 16..254 are legal, if a platform really wants to push it.
   * (opcode) can be Channel Voice or System events, and a few extras:
   *   0x00 "goodbye", channel disconnected. Platform should also send a 0xff Reset on disconnect.
   *   0x01 "hello", channel connected.
   * (a,b) depend on opcode. eg for 0x80 Note Off and 0x90 Note On, they are noteid and velocity.
   * You can't receive SysEx events, by design.
   */
  void (*midi_in)(uint8_t chid,uint8_t opcode,uint8_t a,uint8_t b);
  
} gamek_client;

#ifdef __cplusplus
  }
#endif

#endif
