import { GamekVideo } from "./GamekVideo.js";
import { GamekAudio } from "./GamekAudio.js";
import { GamekInput } from "./GamekInput.js";
import { GamekFiles } from "./GamekFiles.js";

export class GamekController {
  constructor(audioRate) {
    this.bin = null;
    this.module = null;
    this.instance = null;
    this.initialized = false;
    this.running = false;
    this.terminated = false;
    this.title = "untitled";
    this.enableLogging = true;
    this.songp = 0; // record address and length of current song, don't restart the current one.
    this.songc = 0;
    this.video = new GamekVideo(this);
    this.audio = new GamekAudio(this, audioRate);
    this.input = new GamekInput(this);
    this.files = new GamekFiles(this);
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
    this.video.installInDom(canvas);
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
    this.video.resume();
    this.audio.resume();
    this.input.resume();
  }
  
  end() {
    this.running = false;
    this.video.suspend();
    this.audio.suspend();
    this.input.suspend();
  }
  
  // Triggered by GamekVideo; it is responsible for requestAnimationFrame().
  update() {
    if (!this.running) return false;
    this.instance.exports.gamek_web_client_update();
    if (this.instance.exports.gamek_web_client_render()) {
      this.video.applyFramebuffer();
    }
    this.audio.update();
    this.input.update();
    return true;
  }
  
  /* Platform API and some extras, exposed to Wasm app.
   ****************************************************************************/
  
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
    this.terminated = true;
    this.end();
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
    const file = this.files.getFile(this.readStaticCString(path), false);
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
    const file = this.files.getFile(this.readStaticCString(path), true);
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
    const file = this.files.getFile(this.readStaticCString(path), true);
    if (!file) return -1;
    const dstView = file.getView(seek, srcView.byteLength, true);
    if (!dstView) return -1;
    dstView.set(srcView);
    file.persist();
    return dstView.byteLength;
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
    if ((w < 1) || (h < 1)) {
      throw new Error(`Invalid framebuffer size ${w},${h}`);
    }
    const fbSize = w * h * 4;
    const fb = this.getMemoryView(v, fbSize);
    this.video.setFramebuffer(fb, w, h);
  }
  
  _gamek_web_external_set_title(v) {
    try {
      const title = this.readStaticCString(v);
      if (!title) return;
      this.title = title;
    } catch (e) {}
  }
  
  /* Access to Wasm app's memory.
   ****************************************************************/
  
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
  
  /* Callbacks from subsystems.
   ***********************************************************/
  
  onInput(playerid, btnid, v) {
    if (!this.running) return;
    this.instance.exports.gamek_web_client_input_event(playerid, btnid, v);
  }
}
