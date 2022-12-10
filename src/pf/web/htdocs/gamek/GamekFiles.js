export class GamekFiles {
  constructor(controller) {
    this.controller = controller;
    this.readLocalStorage = true;
    this.writeLocalStorage = true;
    this.files = []; // GamekFile
  }
  
  getFile(path, create) {
    const key = this.composeFileKey(path);
    if (!key) return null;
    let file = this.files.find(f => f.key === key);
    if (file) return file;
    
    // Look up in localStorage (this doesn't count as 'create'ing).
    if (this.readLocalStorage) {
      const serial = window.localStorage.getItem(key);
      if (serial) {
        file = new GamekFile(key, this);
        try {
          file.decode(serial);
        } catch (e) {
          if (this.writeLocalStorage) window.localStorage.removeItem(key);
          if (!create) return null;
        }
        this.files.push(file);
        return file;
      }
    }
    
    if (!create) return null;
    file = new GamekFile(key, this);
    this.files.push(file);
    return file;
  }
  
  composeFileKey(path) {
    if (!path) return null;
    if (!this.controller.title) return null;
    return this.controller.title + ":" + path;
  }
}

export class GamekFile {
  constructor(key, files) {
    this.key = key;
    this.files = files;
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
    if (!this.files.writeLocalStorage) return;
    const serial = new TextDecoder("iso-8859-1").decode(this.buffer);
    window.localStorage.setItem(this.key, serial);
  }
}
