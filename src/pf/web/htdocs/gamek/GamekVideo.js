export class GamekVideo {
  constructor(controller) {
    this.controller = controller;
    this.FRAMEBUFFER_SIZE_LIMIT = 1024; // Arbitrary.
    this.canvas = null;
    this.context = null;
    this.imageData = null;
    this.fb = null; // Uint8Array pointing into module's memory
    this.fbw = 0;
    this.fbh = 0;
    this.running = false;
    this.frameRequestPending = false;
  }
  
  installInDom(canvas) {
    this.canvas = canvas;
    const previous = canvas._gamek_controller;
    if (previous) {
      previous.end();
    }
    canvas._gamek_controller = this.controller;
  }
  
  resume() {
    if (this.running) return;
    this.running = true;
    if (!this.frameRequestPending) {
      this.frameRequestPending = true;
      window.requestAnimationFrame(() => this.update());
    }
  }
  
  suspend() {
    if (!this.running) return;
    this.running = false;
  }
  
  update() {
    this.frameRequestPending = false;
    if (!this.running) return;
    if (!this.controller.update()) return;
    this.frameRequestPending = true;
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
  
  // Should happen just once, during init. But more is not a problem.
  setFramebuffer(fb, fbw, fbh) {
    this.fb = fb;
    this.fbw = fbw;
    this.fbh = fbh;
    this.imageData = null;
  }
}
