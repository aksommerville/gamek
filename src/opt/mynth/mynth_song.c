#include "mynth_internal.h"

/* Update, main entry point.
 */

void mynth_song_update() {
  while (1) {
    if (mynth.songdelay) return;
    if (!mynth.song) return;
  
    // End of song: Return to loopp and report a 1-frame delay.
    if ((mynth.songp>=mynth.songc)||!mynth.song[mynth.songp]) {
      mynth_release_all();
      mynth.songp=mynth.songloopp;
      mynth.songdelay=1;
      return;
    }
    uint8_t lead=mynth.song[mynth.songp++];
    
    // Short delay: 0nnnnnnn
    if (!(lead&0x80)) {
      mynth.songdelay=lead*mynth.songtempo;
      return;
    }
    
    // All other events are identified by the top 4 bits of the leading byte.
    switch (lead&0xf0) {
      case 0x80: { // Long delay: 1000nnnn nnnnnnnn
          if (mynth.songp>mynth.songc-1) goto _invalid_;
          mynth.songdelay=((lead&0x0f)<<8)|mynth.song[mynth.songp];
          mynth.songp++;
          if (mynth.songdelay) mynth.songdelay*=mynth.songtempo;
          else mynth.songdelay=1;
          return;
        }
      case 0x90: { // Once Note: 1001cccc NNNNNNNv vvDDDDDD
          if (mynth.songp>mynth.songc-2) goto _invalid_;
          uint8_t chid=lead&0x0f;
          uint8_t noteid=mynth.song[mynth.songp]>>1;
          uint8_t velocity=(mynth.song[mynth.songp]<<6)|(mynth.song[mynth.songp+1]>>2);
          velocity&=0x70;
          velocity|=velocity>>3;
          velocity|=velocity>>6;
          uint32_t ttl=(mynth.song[mynth.songp+1]&0x3f)*mynth.songtempo;
          mynth.songp+=2;
          mynth_note_once(chid,noteid,velocity,ttl);
        } break;
      case 0xa0: { // Note On: 1010cccc 0nnnnnnn 0vvvvvvv
          if (mynth.songp>mynth.songc-2) goto _invalid_;
          uint8_t chid=lead&0x0f;
          uint8_t noteid=mynth.song[mynth.songp++];
          uint8_t velocity=mynth.song[mynth.songp++];
          if (chid<MYNTH_CHANNEL_LIMIT) {
            mynth_note_on(mynth.channelv+chid,noteid,velocity);
          }
        } break;
      case 0xb0: { // Note Off: 1011cccc 0nnnnnnn
          if (mynth.songp>mynth.songc-1) goto _invalid_;
          uint8_t chid=lead&0x0f;
          uint8_t noteid=mynth.song[mynth.songp++];
          if (chid<MYNTH_CHANNEL_LIMIT) {
            mynth_note_off(mynth.channelv+chid,noteid,0x40);
          }
        } break;
      case 0xc0: { // Control Change: 1100cccc kkkkkkkk vvvvvvvv
          if (mynth.songp>mynth.songc-2) goto _invalid_;
          uint8_t chid=lead&0x0f;
          uint8_t k=mynth.song[mynth.songp++];
          uint8_t v=mynth.song[mynth.songp++];
          if (chid<MYNTH_CHANNEL_LIMIT) {
            mynth_channel_control(mynth.channelv+chid,k,v);
          }
        } break;
      case 0xd0: { // Pitch wheel: 1101dddd vvvvvvvv
          if (mynth.songp>mynth.songc-1) goto _invalid_;
          uint8_t chid=lead&0x0f;
          uint8_t v=mynth.song[mynth.songp++];
          if (chid<MYNTH_CHANNEL_LIMIT) {
            mynth_wheel(mynth.channelv+chid,(v<<6)|(v&0x3f));
          }
        } break;
      default: _invalid_: {
          mynth_play_song(0,0);
          return;
        }
    }
  }
}
