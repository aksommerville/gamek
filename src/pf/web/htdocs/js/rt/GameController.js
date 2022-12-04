import { Dom } from "../Dom.js";
import { WasmAdapter } from "./WasmAdapter.js";
import { Runtime } from "./Runtime.js";

export class GameController {
  static getDependencies() {
    return [HTMLCanvasElement, Dom, WasmAdapter, Runtime];
  }
  constructor(element, dom, wasmAdapter, runtime) {
    this.element = element;
    this.dom = dom;
    this.wasmAdapter = wasmAdapter;
    this.runtime = runtime;
    
    this.context = null;
    this.imageData = null;
    
    this.runtime.onFramebufferReady = (fb, w, h) => this.onFramebufferReady(fb, w, h);
  }
  
  load(path) {
    this.runtime.suspend();
    this.wasmAdapter.load(path).then((instance) => {
      console.log(`loaded wasm module`, instance);
      this.runtime.loaded = true;
      this.runtime.resume();
    }).catch((error) => {
      console.error(error);
    });
  }
  
  onFramebufferReady(fb, w, h) {
    if (fb.length !== w * h *4) {
      throw new Error(`Receiving framebuffer: w=${w} * h=${h} * 4 = ${w * h * 4}, fb.length=${fb.length}`);
    }
    this._requireContext(w, h);
    this.imageData.data.set(fb);
    this.context.putImageData(this.imageData, 0, 0);
  }
  
  _requireContext(w, h) {
    if (this.imageData && (w === this.element.width) && (h === this.element.height)) {
      return;
    }
    this.element.width = w;
    this.element.height = h;
    this.context = this.element.getContext("2d");
    this.imageData = this.context.createImageData(w, h);
  }
}
