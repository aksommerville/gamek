/* Gamek.js
 * Browser runtime environment for games written with the Gamek framework.
 */
 
/* Public.
 ***********************************************************************/

/* Create a new Gamek runtime using the Wasm file (bin), and attach it to <canvas> element (canvas).
 * If another runtime was previously attached to this element, we drop it gracefully.
 * Returns a Promise that resolves to the new GamekController, once it's running.
 */
export function gamek_init(canvas, bin) {
  const controller = new GamekController();
  return controller.loadBinary(bin).then(() => {
    controller.installInDom(canvas);
    controller.begin();
    return controller;
  });
}

/* Private.
 *****************************************************************************/
 
class GamekController {
  constructor() {
    this.FRAMEBUFFER_SIZE_LIMIT = 1024; // Arbitrary.
    this.canvas = null;
    this.bin = null;
    this.module = null;
    this.instance = null;
    this.initialized = false;
    this.running = false;
    this.terminated = false;
    this.fb = null; // Uint8Array pointing into module's memory
    this.fbw = 0;
    this.fbh = 0;
    this.title = "untitled";
    this.context = null;
    this.imageData = null;
    this.enableLogging = true; // Print to console when the game does fprintf.
    this.audio = new GamekAudio();
    this.songp = null; // Track most recent song play so we can discard redundant ones.
    this.songc = null;
    this.input = new GamekInput();
    this.input.cb = (playerid, btnid, v) => this.onInput(playerid, btnid, v);
    this.files = [];
  }
  
  loadBinary(bin) {
    this.bin = bin;
    return WebAssembly.instantiate(this.bin, this.generateOptions())
      .then((result) => {
        this.module = result.module;
        this.instance = result.instance;
        return result;
      });
  }
  
  installInDom(canvas) {
    this.canvas = canvas;
    const previous = canvas._gamek_controller;
    if (previous) {
      previous.end();
    }
    canvas._gamek_controller = this;
  }
  
  begin() {
    if (!this.instance) throw new Error(`Wasm instance not loaded`);
    if (this.running) return;
    if (this.terminated) throw new Error(`Wasm program has terminated`);
    this.running = true;
    if (!this.initialized) {
      if (this.instance.exports.gamek_web_client_init() < 0) {
        this.terminated = true;
        throw new Error(`Error initializing game.`);
      }
      this.initialized = true;
    }
    this.audio.resume();
    this.input.resume();
    window.requestAnimationFrame(() => this.update());
  }
  
  end() {
    this.running = false;
    this.audio.suspend();
    this.input.suspend();
  }
  
  update() {
    if (!this.running) return;
    this.instance.exports.gamek_web_client_update();
    if (this.instance.exports.gamek_web_client_render()) {
      this.applyFramebuffer();
    }
    this.audio.updateSong();
    window.requestAnimationFrame(() => this.update());
  }
  
  applyFramebuffer() {
    if (!this.fb) return;
    if (!this.canvas) return;
    if (!this.context) {
      this.context = this.canvas.getContext("2d");
    }
    if (!this.imageData) {
      this.imageData = this.context.createImageData(this.fbw, this.fbh);
      this.canvas.width = this.fbw;
      this.canvas.height = this.fbh;
    }
    this.imageData.data.set(this.fb);
    this.context.putImageData(this.imageData, 0, 0);
  }
  
  generateOptions() {
    return {
      env: {
        gamek_platform_terminate: () => this._gamek_platform_terminate(),
        gamek_platform_audio_event: (chid, opcode, a, b) => this.audio.event(chid, opcode, a, b),
        gamek_platform_play_song: (v, c) => this._gamek_platform_play_song(v, c),
        gamek_platform_set_audio_wave: (waveid, v, c) => this._gamek_platform_set_audio_wave(waveid, v, c),
        gamek_platform_audio_configure: (v, c) => this._gamek_platform_audio_configure(v, c),
        gamek_platform_file_read: (dst, dsta, path, seek) => this._gamek_platform_file_read(dst, dsta, path, seek),
        gamek_platform_file_write_all: (path, src, srcc) => this._gamek_platform_file_write_all(path, src, srcc),
        gamek_platform_file_write_part: (path, seek, src, srcc) => this._gamek_platform_file_write_part(path, seek, src, srcc),
        
        gamek_web_external_console_log: (src) => this._gamek_web_external_console_log(src),
        gamek_web_external_set_title: (v) => this._gamek_web_external_set_title(v),
        gamek_web_external_set_framebuffer: (v, w, h) => this._gamek_web_external_set_framebuffer(v, w, h),
      },
    };
  }
  
  _gamek_platform_terminate() {
    this.running = false;
    this.terminated = true;
  }
  
  _gamek_platform_play_song(v, c) {
    if ((v === this.songp) && (c === this.songc)) return;
    this.songp = v;
    this.songc = c;
    const src = this.copyMemoryView(v, c);
    this.audio.playSong(src);
  }
  
  _gamek_platform_set_audio_wave(waveid, v, c) {
    const src = this.getMemoryView(v, c * 2);
    this.audio.setWave(waveid, src);
  }
  
  _gamek_platform_audio_configure(v, c) {
    const src = this.getMemoryView(v, c);
    this.audio.configure(src);
  }
  
  _gamek_platform_file_read(dst, dsta, path, seek) {
    const dstView = this.getMemoryView(dst, dsta);
    if (!dstView) return -1;
    const file = this.getFile(path, false);
    if (!file) return -1;
    const srcView = file.getView(seek, dsta, false);
    if (!srcView) return -1;
    const dstc = Math.min(dstView.byteLength, srcView.byteLength);
    dstView.set(srcView, 0, dstc);
    return dstc;
  }
  
  _gamek_platform_file_write_all(path, src, srcc) {
    const srcView = this.getMemoryView(src, srcc);
    if (!srcView) return -1;
    const file = this.getFile(path, true);
    if (!file) return -1;
    file.empty();
    const dstView = file.getView(0, srcView.byteLength, true);
    if (!dstView) return -1;
    dstView.set(srcView);
    file.persist();
    return dstView.byteLength;
  }
  
  _gamek_platform_file_write_part(path, seek, src, srcc) {
    const srcView = this.getMemoryView(src, srcc);
    if (!srcView) return -1;
    const file = this.getFile(path, true);
    if (!file) return -1;
    const dstView = file.getView(seek, srcView.byteLength, true);
    if (!dstView) return -1;
    dstView.set(srcView);
    file.persist();
    return dstView.byteLength;
  }
  
  getFile(pathAddr, create) {
    const key = this.composeFileKey(pathAddr);
    if (!key) return null;
    let file = this.files.find(f => f.key === key);
    if (file) return file;
    
    // Look up in localStorage (this doesn't count as 'create'ing).
    const serial = window.localStorage.getItem(key);
    if (serial) {
      file = new GamekFile(key);
      try {
        file.decode(serial);
      } catch (e) {
        window.localStorage.removeItem(key);
        if (!create) return null;
      }
      this.files.push(file);
      return file;
    }
    
    if (!create) return null;
    file = new GamekFile(key);
    this.files.push(file);
    return file;
  }
  
  _gamek_web_external_console_log(src) {
    if (!this.enableLogging) return;
    try {
      src = this.readStaticCString(src);
    } catch (e) { return; }
    console.log(`[game] ${src}`);
  }
  
  _gamek_web_external_set_framebuffer(v, w, h) {
    const buffer = this.instance.exports.memory.buffer;
    if (
      (w < 1) || (w > this.FRAMEBUFFER_SIZE_LIMIT) ||
      (h < 1) || (h > this.FRAMEBUFFER_SIZE_LIMIT)
    ) throw new Error(`Invalid framebuffer size ${w},${h}`);
    const fbSize = w * h * 4;
    if ((v < 0) || (v > buffer.byteLength - fbSize)) {
      throw new Error(`Invalid framebuffer address ${v} dim=${w},${h} memory=${buffer.byteLength}`);
    }
    this.fb = new Uint8Array(buffer, v, fbSize);
    this.fbw = w;
    this.fbh = h;
    this.imageData = null;
  }
  
  _gamek_web_external_set_title(v) {
    try {
      const title = this.readStaticCString(v);
      if (!title) return;
      this.title = title;
    } catch (e) {}
  }
  
  getMemoryView(v, c) {
    if (!this.instance) throw new Error(`Module not loaded`);
    const memory = this.instance.exports.memory.buffer;
    if ((typeof(v) !== "number") || (typeof(c) !== "number") || (v < 0) || (c < 0) || (v > memory.byteLength - c)) {
      throw new Error(`Invalid address ${c} @ ${v}`);
    }
    return new Uint8Array(memory, v, c);
  }
  
  copyMemoryView(v, c) {
    const src = this.getMemoryView(v, c);
    const dst = new Uint8Array(src.length);
    dst.set(src);
    return dst;
  }
  
  readStaticCString(nameOrAddress) {
    const SANITY_LIMIT = 8 << 10;
    if (!this.instance) throw new Error(`Can't look up wasm member '${nameOrAddress}', module not loaded.`);
    let p;
    if (typeof(nameOrAddress) === "string") {
      const g = this.instance.exports[nameOrAddress];
      if (!g) throw new Error(`Wasm export '${nameOrAddress}' not found.`);
      p = g.value;
    } else if (typeof(nameOrAddress) === "number") {
      p = nameOrAddress;
    }
    const buffer = this.instance.exports.memory.buffer;
    if ((typeof(p) !== "number") || (p < 0) || (p >= buffer.byteLength)) {
      throw new Error(`Invalid address for wasm export '${nameOrAddress}'`);
    }
    const fullView = new Uint8Array(buffer);
    let c = 0;
    for (;;) {
      if (p + c >= buffer.byteLength) {
        throw new Error(`Wasm export '${nameOrAddress}' not terminated`);
      }
      if (!fullView[p + c]) break;
      c++;
      if (c > SANITY_LIMIT) {
        throw new Error(`Wasm export '${nameOrAddress}' exceeded sanity limit ${SANITY_LIMIT} bytes.`);
      }
    }
    return new TextDecoder().decode(new Uint8Array(buffer, p, c));
  }
  
  composeFileKey(pathAddr) {
    try {
      if (!this.title) return null;
      const path = this.readStaticCString(pathAddr);
      if (!path) return null;
      return this.title + ":" + path;
    } catch (e) { return null; }
  }
  
  onInput(playerid, btnid, v) {
    if (!this.running) return;
    this.instance.exports.gamek_web_client_input_event(playerid, btnid, v);
  }
}

class GamekFile {
  constructor(key) {
    this.key = key;
    this.buffer = new ArrayBuffer(0);
  }
  
  empty() {
    this.buffer = new ArrayBuffer(0);
  }
  
  decode(serial) {
    const view = new TextEncoder("iso-8859-1").encode(serial);
    this.buffer = view.buffer;
  }
  
  getView(p, c, extend) {
    if (p < 0) return null;
    if (c < 0) return null;
    const reqLimit = p + c;
    if (reqLimit > this.buffer.byteLength) {
      if (extend) {
        const nv = new ArrayBuffer(reqLimit);
        const dstView = new Uint8Array(nv, 0, this.buffer.byteLength);
        const srcView = new Uint8Array(this.buffer);
        dstView.set(srcView);
        this.buffer = nv;
      } else {
        c = this.buffer.byteLength - p;
        if (c < 1) return new Uint8Array(0);
      }
    }
    return new Uint8Array(this.buffer, p, c);
  }
  
  persist() {
    const serial = new TextDecoder("iso-8859-1").decode(this.buffer);
    window.localStorage.setItem(this.key, serial);
  }
}

class GamekAudio {
  constructor() {
    this.CHANNEL_COUNT = 16;
    this.running = false;
    this.song = null;
    this.songTempo = 0; // ms/tick
    this.songStartp = 0;
    this.songLoopp = 0;
    this.songp = 0;
    this.songNextTime = 0; // absolute time in ms of the next event; 0 if we haven't read it yet
    this.channels = [];
    this.voices = [];
    
    this.context = new AudioContext({
      sampleRate: 44010,
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
    this.songNextTime = 0;
  }
  
  resume() {
    if (this.running) return;
    this.running = true;
    this.context.resume();
    if (!this.songNextTime) this.songNextTime = Date.now();
  }
  
  updateSong() {
    this.removeDefunctVoices();
    
    // Unlike most of my C synthesizers, this one has song timing completely independent of the signal graph.
    // Events might miss their mark by up to one video frame.
    // We will manage clocks such that that timing error does not accumulate.
    // Track when (running) goes false, and reset the clock. Otherwise the notes pile up during a pause and all play at once on resume :(
    if (!this.running) {
      this.songNextTime = 0;
      return;
    } else if (!this.songNextTime) {
      this.songNextTime = Date.now();
      return;
    }
    
    for (;;) {
      if (!this.song) return;
      if (this.songNextTime > Date.now()) return;
      
      if (this.songp >= this.song.length) {
        this.releaseAll();
        this.songp = this.songLoopp;
        this.songNextTime = Date.now();
        return;
      }
      
      const lead = this.song[this.songp];
      
      if (!lead) { // EOF
        this.releaseAll();
        this.songp = this.songLoopp;
        this.songNextTime = Date.now();
        return;
      }
      
      if (!(lead & 0x80)) { // Short Delay
        this.songNextTime += lead * this.songTempo;
        this.songp += 1;
        return;
      }
      
      const require = (c) => {
        if (this.songp > this.song.length - c) {
          this.playSong(null);
          return;
        }
      };
      switch (lead & 0xf0) {
        case 0x80: { // Long Delay
            require(2);
            const tickc = ((this.song[this.songp] & 0x0f) << 8) | this.song[this.songp + 1];
            this.songNextTime += lead * this.songTempo;
            this.songp += 2;
            return;
          }
        case 0x90: { // Note Once: 1001cccc NNNNNNNv vvDDDDDD
            require(3);
            const chid = lead & 0x0f;
            const noteid = this.song[this.songp + 1] >> 1;
            let velocity = ((this.song[this.songp + 1] << 6) | (this.song[this.songp + 2] >> 2)) & 0x70;
            velocity |= velocity >> 3;
            velocity |= velocity >> 6;
            const tickc = this.song[this.songp + 2] & 0x3f;
            this.songp += 3;
            this.noteOnce(chid, noteid, velocity, tickc * this.songTempo);
            break;
          }
        case 0xa0: { // Note On: 1010cccc 0nnnnnnn 0vvvvvvv
            require(3);
            const chid = lead & 0x0f;
            const noteid = this.song[this.songp + 1];
            const velocity = this.song[this.songp + 2];
            this.songp += 3;
            this.noteOn(chid, noteid, velocity);
            break;
          }
        case 0xb0: { // Note Off: 1011cccc 0nnnnnnn
            require(2);
            const chid = lead & 0x0f;
            const noteid = this.song[this.songp + 1];
            this.songp += 2;
            this.noteOff(chid, noteid, 0x40);
            break;
          }
        case 0xc0: { // Control: 1100cccc kkkkkkkk vvvvvvvv
            require(3);
            const chid = lead & 0x0f;
            const k = this.song[this.songp + 1];
            const v = this.song[this.songp + 1];
            this.songp += 3;
            this.controlChange(chid, k, v);
            break;
          }
        case 0xd0: { // Wheel: 1101dddd vvvvvvvv
            require(2);
            const chid = lead & 0x0f;
            const v = (this.song[this.songp + 1] << 6) | (this.song[this.songp + 1] & 0x3f);
            this.songp += 2;
            this.pitchWheel(chid, v);
            break;
          }
        default: this.playSong(null); return; // Illegal leading byte.
      }
    }
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
    this.songTempo = src[0];
    this.songStartp = src[1];
    this.songLoopp = (src[2] << 8) | src[3];
    if (!this.songTempo || (this.songStartp < 4) || (this.songLoopp < this.songStartp) || (this.songLoopp >= src.length)) {
      this.endSong();
      return;
    }
    this.songp = this.songStartp;
    this.song = src;
    this.songNextTime = Date.now();
  }
  
  endSong() {
    this.releaseAll();
    this.song = null;
    this.songTempo = 0;
    this.songStartp = 0;
    this.songLoopp = 0;
    this.songp = 0;
    this.songNextTime = 0;
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

class GamekInput {
  constructor() {
    this.running = false;
    this.keyEventListener = null;
    
    // Owner (GamekController) should replace:
    this.cb = (playerid, btnid, value) => {};
    
    // This initial size of playerStates defines the limit of input players we can handle.
    // 4 is sensible, so is 1, and you can go up to 255.
    // The first entry is special, player zero, the aggregate state.
    this.playerStates = [0, 0, 0, 0, 0];
    
    this.keyMap = {//TODO must be configurable
      ArrowLeft: [1, GamekInput.BUTTON_LEFT],
      ArrowRight: [1, GamekInput.BUTTON_RIGHT],
      ArrowUp: [1, GamekInput.BUTTON_UP],
      ArrowDown: [1, GamekInput.BUTTON_DOWN],
      KeyZ: [1, GamekInput.BUTTON_SOUTH],
      KeyX: [1, GamekInput.BUTTON_WEST],
      KeyA: [1, GamekInput.BUTTON_EAST],
      KeyS: [1, GamekInput.BUTTON_NORTH],
      Enter: [1, GamekInput.BUTTON_AUX1],
      Tab: [1, GamekInput.BUTTON_AUX2],
      Space: [1, GamekInput.BUTTON_AUX3],
      Backquote: [1, GamekInput.BUTTON_L1],
      Digit1: [1, GamekInput.BUTTON_L2],
      Backspace: [1, GamekInput.BUTTON_R1],
      Equal: [1, GamekInput.BUTTON_R2],
    };
    
    //TODO joysticks
    //TODO touch
    //TODO midi
  }
  
  suspend() {
    if (!this.running) return;
    this.running = false;
    if (this.keyEventListener) {
      window.removeEventListener("keydown", this.keyEventListener);
      window.removeEventListener("keyup", this.keyEventListener);
      this.keyEventListener = null;
    }
  }
  
  resume() {
    if (this.running) return;
    this.running = true;
    this.keyEventListener = (event) => this.onKey(event);
    window.addEventListener("keydown", this.keyEventListener);
    window.addEventListener("keyup", this.keyEventListener);
  }
  
  sendPlayerEvent(playerid, btnid, v) {
    //console.log(`TODO GamekInput.sendPlayerEvent ${playerid} ${btnid} ${v}`);
    this.cb(playerid, btnid, v);
  }
  
  setPlayerButton(playerid, btnid, v) {
    if (playerid < 0) return;
    if (playerid >= this.playerStates.length) return;
    
    if (v) {
      if (this.playerStates[playerid] & btnid) return;
      if (!(this.playerStates[playerid] & GamekInput.BUTTON_CD)) {
        this.playerStates[playerid] |= GamekInput.BUTTON_CD;
        this.sendPlayerEvent(playerid, GamekInput.BUTTON_CD, 1);
      }
      this.playerStates[playerid] |= btnid;
      this.sendPlayerEvent(playerid, btnid, 1);
    } else {
      if (!(this.playerStates[playerid] & btnid)) return;
      this.playerStates[playerid] &= ~btnid;
      this.sendPlayerEvent(playerid, btnid, 0);
    }
    
    // If it's a nonzero player, repeat the event against player zero.
    if (playerid) {
      this.setPlayerButton(0, btnid, v);
    }
  }
  
  onKey(event) {
    if (event.repeat) return;
    const map = this.keyMap[event.code];
    if (!map) return;
    const v = (event.type === "keydown") ? 1 : 0;
    this.setPlayerButton(map[0], map[1], v);
    event.preventDefault();
  }
}

// These must match the constants in src/pf/gamek_pf.h
GamekInput.BUTTON_LEFT    = 0x0001;
GamekInput.BUTTON_RIGHT   = 0x0002;
GamekInput.BUTTON_UP      = 0x0004;
GamekInput.BUTTON_DOWN    = 0x0008;
GamekInput.BUTTON_SOUTH   = 0x0010;
GamekInput.BUTTON_WEST    = 0x0020;
GamekInput.BUTTON_EAST    = 0x0040;
GamekInput.BUTTON_NORTH   = 0x0080;
GamekInput.BUTTON_L1      = 0x0100;
GamekInput.BUTTON_R1      = 0x0200;
GamekInput.BUTTON_L2      = 0x0400;
GamekInput.BUTTON_R2      = 0x0800;
GamekInput.BUTTON_AUX1    = 0x1000;
GamekInput.BUTTON_AUX2    = 0x2000;
GamekInput.BUTTON_AUX3    = 0x4000;
GamekInput.BUTTON_CD      = 0x8000;
