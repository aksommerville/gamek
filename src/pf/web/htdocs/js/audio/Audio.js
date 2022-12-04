/* Audio.js
 * Top level audio manager.
 * Responsible for the Web Audio API relationship.
 */
 
import { AudioConverter } from "./AudioConverter.js";
import { SongPlayer } from "./SongPlayer.js";
import { BasicChannel } from "./BasicChannel.js";
import { Wave } from "./Wave.js";

/* interface Channel {
  constructor(audio)
  configure(k, v)
  noteOn(noteid, freq: hz, velocity: 0..1, cbFinish): Voice | null
} */

/* interface Voice {
  chid: number // assigned by owner
  noteid: number // assigned by owner
  node: AudioNode
  release?() // exception, we drop the voice cold
} */
 
export class Audio {
  static getDependencies() {
    return [AudioConverter, Window];
  }
  constructor(audioConverter, window) {
    this.audioConverter = audioConverter;
    this.window = window;
    
    this.context = null; // AudioContext. Delay construction until after a user gesture, it's a browser thing.
    this.songPlayer = null; // SongPlayer, if a song is playing.
    this.voices = [];
    this.channels = []; // indexed by chid 0..15. Channel.
    this.waves = [];
  }
  
  /* Master controls.
   ***************************************************************************/
  
  suspend() {
    this.songPlayer?.suspend();
    this.context?.suspend();
  }
  
  resume() {
    if (!this.context) {
      if (!this.window.AudioContext) {
        this.window.console.log(`Looks like Web Audio API is not available.`);
        return;
      }
      this.context = new this.window.AudioContext({
        sampleRate: 22050,
        latencyHint: "interactive",
      });
    }
    if (this.context.state === "suspended") this.context.resume();
    this.songPlayer?.resume();
  }
  
  /* Public API, visible to Wasm app.
   ***************************************************************************/
   
  decodeWaves(src) {
    this.waves = Wave.decodeAll(src);
  }
   
  configure(serial) {
    console.log(`TODO Audio.configure`, serial);
    this._rebuildChannels();
  }
   
  playSong(serial) {
    if (this.songPlayer) {
      this.songPlayer.suspend();
      this.songPlayer = null;
    }
    if (serial && (serial.length >= 4)) {
      this._rebuildChannels();
      this.songPlayer = new SongPlayer(this.window, serial, {
        config: (chid, k, v) => this.channelConfig(chid, k, v),
        note: (chid, noteid, velocity, duration) => this.note(chid, noteid, velocity, duration),
        noteOn: (chid, noteid, velocity) => this.noteOn(chid, noteid, velocity),
        noteOff: (chid, noteid) => this.noteOff(chid, noteid),
      });
      if (this.context?.state === "running") this.songPlayer.resume();
    }
  }
   
  pauseSong(pause) {
    // Playing a new song, or pausing the global Audio state, will drop this application pause state.
    // I think that's OK? This really only happens when violin encoding is in progress; one is unlikely to pause or change settings during encode.
    if (pause) this.songPlayer?.suspend();
    else this.songPlayer?.resume();
  }
  
  channelConfig(chid, k, v) {
    if ((chid < 0) || (chid >= 16)) return;
    if ((k < 0) || (k >= 256)) return;
    if (!k) {
      // Config 0 (v ignored) means reset channel.
      this.channels[chid] = this._generateChannel(chid);
    } else {
      // Everything else is processed by the channel itself.
      const channel = this.channels[chid];
      if (channel) channel.configure(k, v);
    }
  }
  
  note(chid, noteid, velocity, durationms) {
    if (!this.noteOn(chid, noteid, velocity)) return false;
    this.window.setTimeout(() => this.noteOff(chid, noteid), durationms);
    return true;
  }
  
  noteOn(chid, noteid, velocity) {
    if (!this.context) return false;
    if (this.context.state !== "running") return false;
    const channel = this.channels[chid];
    if (!channel) return false;
    velocity = this.audioConverter.normalizeVelocity(velocity);
    const freq = this.audioConverter.frequencyFromNoteid(noteid);
    let prefinish = false;
    const voice = channel.noteOn(noteid, freq, velocity, () => {
      prefinish = true;
      const p = this.voices.indexOf(voice);
      if (p >= 0) {
        this.voices.splice(p, 1);
        voice.node.disconnect();
      }
    });
    if (!voice) return false;
    if (!prefinish) {
      voice.chid = chid;
      voice.noteid = noteid;
      this.voices.push(voice);
      voice.node.connect(this.context.destination);
    }
    return true;
  }
  
  noteOff(chid, noteid) {
    for (let i=this.voices.length; i-->0; ) {
      const voice = this.voices[i];
      if ((voice.chid === chid) && (voice.noteid === noteid)) {
        voice.chid = -1;
        voice.noteid = -1;
        try {
          voice.release();
        } catch (e) {
          console.log(e);
          this.voices.splice(i, 1);
          voice.node.disconnect();
        }
      }
    }
  }
  
  silence() {
    for (const voice of this.voices) {
      voice.node.disconnect();
    }
    this.voices = [];
  }
  
  releaseAll() {
    for (let i=this.voices.length; i-->0; ) {
      const voice = this.voices[i];
      try {
        voice.release();
      } catch (e) {
        this.voices.splice(i, 1);
        voice.node.disconnect();
      }
    }
  }
  
  /* Private.
   **************************************************************/
   
  _rebuildChannels() {
    this.channels = [];
    //TODO Use the serial config.
    for (let chid=0; chid<16; chid++) {
      const channel = this._generateChannel(chid);
      this.channels.push(channel);
    }
  }
  
  _generateChannel(chid) {
    return new BasicChannel(this);
  }
}

Audio.singleton = true;
