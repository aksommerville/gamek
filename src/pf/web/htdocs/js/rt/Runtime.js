/* Runtime.js
 * Top-level coordinator for the game's environment.
 */
 
import { Audio } from "../audio/Audio.js";
import { WasmAdapter } from "./WasmAdapter.js";
 
export class Runtime {
  static getDependencies() {
    return [Audio, WasmAdapter, Window];
  }
  constructor(audio, wasmAdapter, window) {
    this.audio = audio;
    this.wasmAdapter = wasmAdapter;
    this.window = window;
    
    // Whoever owns the UI should replace this:
    this.onFramebufferReady = (fb, w, h) => {};
    
    this.running = false;
    this.loaded = false;
    this.wasmInitialized = false;
    this._input = 0;
  }
  
  setInput(input) {
    this._input = input;
  }
  
  /* Hard pause.
   * When paused, the Wasm app does not update.
   * Note that pausing the runtime does not pause audio.
   ********************************************************************/
  
  suspend() {
    if (!this.running) return;
    this.running = false;
  }
  
  resume() {
    if (this.running) return true;
    if (!this.loaded) return false;
    this.running = true;
    if (!this.wasmInitialized) {
      this.wasmAdapter.init();
      this.wasmInitialized = true;
    }
    this.window.requestAnimationFrame(() => this._update());
    return true;
  }
  
  /* Update.
   *******************************************************************/
   
  _update() {
    if (!this.running) return;
    this.wasmAdapter.update(this._input);
    const fb = this.wasmAdapter.getFramebufferIfDirty();
    if (fb) this.onFramebufferReady(fb.fb, fb.w, fb.h);
    this.window.requestAnimationFrame(() => this._update());
  }
}

Runtime.singleton = true;
