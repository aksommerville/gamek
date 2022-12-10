export class GamekAudio {
  constructor(controller, rate) {
    this.controller = controller;
    this.CHANNEL_COUNT = 16;
    this.running = false;
    this.song = null; // GamekSongPlayer
    this.channels = [];
    this.voices = [];
    
    this.context = new AudioContext({
      sampleRate: rate || 44010,
      latencyHint: "interactive",
    });
    for (let i=0; i<this.CHANNEL_COUNT; i++) {
      this.channels.push(new GamekAudioChannel(i, this.context));
    }
  }
  
  suspend() {
    if (!this.running) return;
    this.running = false;
    this.releaseAll();
    this.context.suspend();
    if (this.song) this.song.suspend();
  }
  
  resume() {
    if (this.running) return;
    this.running = true;
    this.context.resume();
    if (this.song) this.song.resume();
  }
  
  update() {
    this.removeDefunctVoices();
    if (this.song) this.song.update();
  }
  
  // (src) may be volatile, as long as it's constant through this call.
  configure(src) {
    console.log(`GamekAudio.configure`, src);//TODO
  }
  
  // (src) may be volatile, as long as it's constant through this call.
  setWave(waveid, src) {
    console.log(`GamekAudio.setWave`, { waveid, src });//TODO
  }
  
  // (src) must be immutable
  playSong(src) {
    this.endSong();
    if (!src || (src.length <= 4)) return;
    this.song = new GamekSongPlayer(src, this);
  }
  
  endSong() {
    this.releaseAll();
    this.song = null;
  }
  
  removeDefunctVoices() {
    for (let i=this.voices.length; i-->0; ) {
      const voice = this.voices[i];
      if (voice.isFinished()) {
        voice.silence();
        this.voices.splice(i, 1);
      }
    }
  }
  
  reset() {
    this.silenceAll();
    for (let chid=0; chid<this.CHANNEL_COUNT; chid++) {
      this.resetChannel(chid);
    }
  }
  
  resetChannel(chid) {
    const channel = this.channels[chid];
    if (channel) {
      channel.reset();
    }
  }
  
  silenceAll() {
    for (const voice of this.voices) {
      voice.silence();
    }
    this.voices = [];
  }
  
  silenceChannel(chid) {
    for (let i=this.voices.length; i-->0; ) {
      const voice = this.voices[i];
      if (voice.chid !== chid) continue;
      voice.silence();
      this.voices.splice(i, 1);
    }
  }
  
  releaseAll() {
    for (const voice of this.voices) {
      voice.release();
    }
  }
  
  releaseChannel(chid) {
    for (const voice of this.voices) {
      if (voice.chid !== chid) continue;
      voice.release();
    }
  }
  
  event(chid, opcode, a, b) {
    
    if (chid === 0xff) {
      switch (opcode) {
        case 0xff: this.reset(); return;
      }
      return;
    }
    
    if ((typeof(chid) !== "number") || (chid < 0) || (chid >= this.CHANNEL_COUNT)) return;
    switch (opcode) {
      case 0x80: this.noteOff(chid, a, b); return;
      case 0x90: this.noteOn(chid, a, b); return;
      case 0xb0: this.controlChange(chid, a, b); return;
      case 0xe0: this.pitchWheel(chid, a | (b << 7)); return;
    }
  }
  
  noteOff(chid, noteid, velocity) {
    for (const voice of this.voices) {
      if (voice.chid !== chid) continue;
      if (voice.noteid !== noteid) continue;
      voice.release();
    }
  }
  
  noteOn(chid, noteid, velocity) {
    if (this.context?.state !== "running") return false;
    const channel = this.channels[chid];
    if (!channel) return false;
    const voice = channel.noteOn(this.context.destination, noteid, velocity);
    if (!voice) return false;
    this.voices.push(voice);
    return true;
  }
  
  noteOnce(chid, noteid, velocity, ttlms) {
    if (!this.noteOn(chid, noteid, velocity)) return;
    window.setTimeout(() => this.noteOff(chid, noteid, 0x40), ttlms);
  }
  
  controlChange(chid, k, v) {
    // System-wide control changes will be handled elsewhere, this is only for channel-specific ones.
    // Watch out! The set of controllers we support is meant to match our other synthesizer, Mynth.
    // It doesn't match standard MIDI exactly.
    
    // A few events are handled here, regardless of the channel's implementation.
    switch (k) {
      case 0x78: this.silenceForChannel(chid); return;
      case 0x79: this.resetChannel(chid); return;
      case 0x7b: this.releaseChannel(chid); return;
    }
    
    const channel = this.channels[chid];
    if (!channel) return;
    channel.controlChange(k, v);
  }
  
  pitchWheel(chid, v) {
    const channel = this.channels[chid];
    if (!channel) return;
    channel.pitchWheel(v);
  }
}

/* GamekAudioChannel
 ******************************************************/

class GamekAudioChannel {
  constructor(chid, context) {
    this.chid = chid;
    this.context = context;
    this.reset();
  }
  
  reset() {
    this.volume = 0.5; // 0..1
    this.wheel = 0x2000; // 0..0x3fff, as received over bus
    this.wheelRange = 2.0; // semitones
    this.bend = 1.0; // derived from (wheel,wheelRange)
    this.waveid = 0;
    this.sustain = true;
    this.warbleRange = 0.0; // semitones
    this.warbleRate = 0; // hz
    
    this.atkclo = this.atkchi =  15; // ms
    this.decclo = this.decchi =  40; // ms
    this.rlsclo = this.rlschi = 200; // ms
    this.atkvlo = this.atkvhi = 0.300;
    this.susvlo = this.susvhi = 0.200;
  }
  
  // Return a new GamekAudioVoice on success, and attach it to (destination).
  noteOn(destination, noteid, velocity) {
    const voice = new GamekAudioVoice(this.context);
    voice.chid = this.chid;
    voice.noteid = noteid;
    if (!voice.setup(noteid, velocity, this)) return null;
    voice.node.connect(destination);
    voice.destination = destination;
    return voice;
  }
  
  controlChange(k, v) {
    switch (k) {
      case 0x07: this.volume = Math.max(Math.min(v / 127.0, 1), 0); return;
      // "lo" envelope fields also set their "hi" counterpart. User should set lo first.
      case 0x0c: this.atkclo = this.atkchi = v; return;
      case 0x0d: this.atkvlo = this.atkvhi = Math.max(Math.min(v / 127.0, 1), 0); return;
      case 0x0e: this.decclo = this.decchi = v; return;
      case 0x0f: this.susvlo = this.susvhi = Math.max(Math.min(v / 127.0, 1), 0); return;
      case 0x10: this.rlsclo = this.rlschi = v * 8; return;
      case 0x2c: this.atkchi = v; return;
      case 0x2d: this.atkvhi = Math.max(Math.min(v / 127.0, 1), 0); return;
      case 0x2e: this.decchi = v; return;
      case 0x2f: this.susvhi = Math.max(Math.min(v / 127.0, 1), 0); return;
      case 0x30: this.rlschi = v; return;
      case 0x40: this.sustain = (v >= 0x40); return;
      case 0x46: return; // Wave select TODO
      case 0x47: this.wheelRange = v / 10; return;
      case 0x48: this.warbleRange = v / 10; return;
      case 0x49: this.warbleRate = v; return;
    }
  }
  
  pitchWheel(v) {
    //TODO
  }
}

/* GamekAudioVoice
 *********************************************************************/

class GamekAudioVoice {
  constructor(context) {
    this.context = context;
    this.chid = null;
    this.noteid = null;
    this.destination = null;
    this.node = null; // our main node, the one connected to destination
    this.oscillator = null;
    this.endTime = null;
  }
  
  setup(noteid, velocity, channel) {
  
    const frequency = 440 * 2 ** ((noteid - 0x45) / 12);
    const type = "sine";//TODO
    this.oscillator = new OscillatorNode(this.context, { frequency, type });
    
    this.node = new GainNode(this.context);
    this.node.gain.setValueAtTime(0, this.context.currentTime);
    //TODO envelope
    this.node.gain.linearRampToValueAtTime(0.400, this.context.currentTime + 0.010);
    this.node.gain.linearRampToValueAtTime(0.100, this.context.currentTime + 0.010 + 0.015);
    this.oscillator.connect(this.node);
    
    this.oscillator.start();
    
    return true;
  }
  
  isFinished() {
    if (!this.destination) return true;
    if (!this.endTime) return false;
    if (this.context.currentTime >= this.endTime) return true;
    return false;
  }
  
  silence() {
    if (this.destination) {
      this.node.disconnect(this.destination);
      this.destination = null;
    }
  }
  
  release() {
    //TODO envelope
    this.node.gain.setValueAtTime(this.node.gain.value, this.context.currentTime);
    this.node.gain.linearRampToValueAtTime(0, this.context.currentTime + 0.500);
    this.endTime = this.context.currentTime + 0.500;
    // Once released, a voice becomes unaddressable:
    this.chid = null;
    this.noteid = null;
  }
}

/* GamekSongPlayer
 ****************************************************/
 
class GamekSongPlayer {
  constructor(src, audio) {
    this.audio = audio;
    this.running = true;
    this.nextTime = Date.now();
    this.init(src);
  }
  
  init(src) {
    if (!src || !(src instanceof Uint8Array) || (src.length < 4)) {
      this.audio.playSong(null);
      return;
    }
    this.src = src;
    this.msPerTick = src[0];
    this.srcp = src[1];
    this.loopp = (src[2] << 8) | src[3];
    if (!this.msPerTick || !this.srcp || (this.srcp < 4) || (this.loopp < this.srcp)) {
      this.audio.playSong(null);
    }
  }
  
  suspend() {
    if (!this.running) return;
    this.running = false;
    this.nextTime = 0;
  }
  
  resume() {
    if (this.running) return;
    this.running = true;
    this.nextTime = Date.now();
  }
  
  update() {
    if (!this.running) return;
    for (;;) {
      if (!this.src) return;
      if (this.nextTime > Date.now()) return;
      
      if (this.srcp >= this.src.length) {
        this.audio.releaseAll();
        this.srcp = this.loopp;
        this.nextTime = Date.now();
        return;
      }
      
      const lead = this.src[this.srcp];
      
      if (!lead) { // EOF
        this.audio.releaseAll();
        this.srcp = this.loopp;
        this.nextTime = Date.now();
        return;
      }
      
      if (!(lead & 0x80)) { // Short Delay
        this.nextTime += lead * this.msPerTick;
        this.srcp += 1;
        return;
      }
      
      switch (lead & 0xf0) {
        case 0x80: { // Long Delay
            if (this.srcp > this.src.length - 2) {
              this.audio.playSong(null);
              return;
            }
            const tickc = ((this.srcp[this.srcp] & 0x0f) << 8) | this.src[this.srcp + 1];
            this.nextTime += lead * this.msPerTick;
            this.srcp += 2;
            return;
          }
        case 0x90: { // Note Once: 1001cccc NNNNNNNv vvDDDDDD
            if (this.srcp > this.src.length - 3) {
              this.audio.playSong(null);
              return;
            }
            const chid = lead & 0x0f;
            const noteid = this.src[this.srcp + 1] >> 1;
            let velocity = ((this.src[this.srcp + 1] << 6) | (this.src[this.srcp + 2] >> 2)) & 0x70;
            velocity |= velocity >> 3;
            velocity |= velocity >> 6;
            const tickc = this.src[this.srcp + 2] & 0x3f;
            this.srcp += 3;
            this.audio.noteOnce(chid, noteid, velocity, tickc * this.msPerTick);
            break;
          }
        case 0xa0: { // Note On: 1010cccc 0nnnnnnn 0vvvvvvv
            if (this.srcp > this.src.length - 3) {
              this.audio.playSong(null);
              return;
            }
            const chid = lead & 0x0f;
            const noteid = this.src[this.srcp + 1];
            const velocity = this.src[this.srcp + 2];
            this.srcp += 3;
            this.audio.noteOn(chid, noteid, velocity);
            break;
          }
        case 0xb0: { // Note Off: 1011cccc 0nnnnnnn
            if (this.srcp > this.src.length - 2) {
              this.audio.playSong(null);
              return;
            }
            const chid = lead & 0x0f;
            const noteid = this.src[this.srcp + 1];
            this.srcp += 2;
            this.audio.noteOff(chid, noteid, 0x40);
            break;
          }
        case 0xc0: { // Control: 1100cccc kkkkkkkk vvvvvvvv
            if (this.srcp > this.src.length - 3) {
              this.audio.playSong(null);
              return;
            }
            const chid = lead & 0x0f;
            const k = this.src[this.srcp + 1];
            const v = this.src[this.srcp + 1];
            this.srcp += 3;
            this.audio.controlChange(chid, k, v);
            break;
          }
        case 0xd0: { // Wheel: 1101dddd vvvvvvvv
            if (this.srcp > this.src.length - 2) {
              this.audio.playSong(null);
              return;
            }
            const chid = lead & 0x0f;
            const v = (this.src[this.srcp + 1] << 6) | (this.src[this.srcp + 1] & 0x3f);
            this.srcp += 2;
            this.audio.pitchWheel(chid, v);
            break;
          }
        default: this.audio.playSong(null); return; // Illegal leading byte.
      }
    }
  }
}
