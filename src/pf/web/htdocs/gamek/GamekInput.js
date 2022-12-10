export class GamekInput {
  constructor(controller) {
    this.controller = controller;
    this.running = false;
    
    this.keyEventListener = null;
    this.generateDefaultKeyMap();
    
    this.gamepadEventListener = null;
    this.gamepads = []; // id (Gamepad instances evidently are not permanent)
    this.gamepadMaps = []; // [ gamepad index, "axis"|"axislo"|"axishi"|"button", srcbtnid, playerid, dstbtnid, state ]
    this.gamepadTemplates = []; // { id, axes, buttons }
    
    this.touchEventListener = null;
    this.touches = []; // { identifier, btnid, startX, startY, hat:0..7|null, changed }
    this.unchangedDpadTime = 0; // 3 taps in the dpad within 2 seconds is a signal to kill touch input
    this.unchangedDpadCount = 0;
    this.onKillTouchEvents = () => {};
    
    this.midiAccess = null;
    this.initMidi();
    
    // This initial size of playerStates defines the limit of input players we can handle.
    // 4 is sensible, so is 1, and you can go up to 255.
    // The first entry is special, player zero, the aggregate state.
    this.playerStates = [0, 0, 0, 0, 0];
    
  }
  
  suspend() {
    if (!this.running) return;
    this.running = false;
    if (this.keyEventListener) {
      window.removeEventListener("keydown", this.keyEventListener);
      window.removeEventListener("keyup", this.keyEventListener);
      this.keyEventListener = null;
    }
    if (this.gamepadEventListener) {
      window.removeEventListener("gamepadconnected", this.gamepadEventListener);
      window.removeEventListener("gamepaddisconnected", this.gamepadEventListener);
      this.gamepadEventListener = null;
    }
    if (this.touchEventListener) {
      window.removeEventListener("touchstart", this.touchEventListener, { passive: false });
      window.removeEventListener("touchend", this.touchEventListener, { passive: false });
      window.removeEventListener("touchcancel", this.touchEventListener, { passive: false });
      window.removeEventListener("touchmove", this.touchEventListener, { passive: false });
      this.touchEventListener = null;
    }
  }
  
  resume() {
    if (this.running) return;
    this.running = true;
    this.keyEventListener = (event) => this.onKey(event);
    window.addEventListener("keydown", this.keyEventListener);
    window.addEventListener("keyup", this.keyEventListener);
    this.gamepadEventListener = (event) => this.onGamepadConnection(event);
    window.addEventListener("gamepadconnected", this.gamepadEventListener);
    window.addEventListener("gamepaddisconnected", this.gamepadEventListener);
    this.touchEventListener = (event) => this.onTouch(event);
    window.addEventListener("touchstart", this.touchEventListener, { passive: false });
    window.addEventListener("touchend", this.touchEventListener, { passive: false });
    window.addEventListener("touchcancel", this.touchEventListener, { passive: false });
    window.addEventListener("touchmove", this.touchEventListener, { passive: false });
  }
  
  update() {
    this.updateGamepads(navigator.getGamepads());
  }
  
  getConfiguration() {
    return {
      keyboard: this.getKeyboardConfiguration(),
      gamepads: this.getGamepadConfiguration(),
      touch: this.getTouchConfiguration(),
      midi: this.getMidiConfiguration(),
    };
  }
  
  setConfiguration(config) {
    if (config) {
      this.setKeyboardConfiguration(config.keyboard);
      this.setGamepadConfiguration(config.gamepads);
      this.setTouchConfiguration(config.touch);
      this.setMidiConfiguration(config.midi);
    } else {
      this.setKeyboardConfiguration(null);
      this.setGamepadConfiguration(null);
      this.setTouchConfiguration(null);
      this.setMidiConfiguration(null);
    }
  }
  
  sendPlayerEvent(playerid, btnid, v) {
    //console.log(`TODO GamekInput.sendPlayerEvent ${playerid} ${btnid} ${v}`);
    this.controller.onInput(playerid, btnid, v);
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
  
  reprButton(btnid) {
    switch (btnid) {
      case GamekInput.BUTTON_LEFT: return "LEFT";
      case GamekInput.BUTTON_RIGHT: return "RIGHT";
      case GamekInput.BUTTON_UP: return "UP";
      case GamekInput.BUTTON_DOWN: return "DOWN";
      case GamekInput.BUTTON_SOUTH: return "SOUTH";
      case GamekInput.BUTTON_WEST: return "WEST";
      case GamekInput.BUTTON_EAST: return "EAST";
      case GamekInput.BUTTON_NORTH: return "NORTH";
      case GamekInput.BUTTON_L1: return "L1";
      case GamekInput.BUTTON_R1: return "R1";
      case GamekInput.BUTTON_L2: return "L2";
      case GamekInput.BUTTON_R2: return "R2";
      case GamekInput.BUTTON_AUX1: return "AUX1";
      case GamekInput.BUTTON_AUX2: return "AUX2";
      case GamekInput.BUTTON_AUX3: return "AUX3";
      case GamekInput.BUTTON_LEFT | GamekInput.BUTTON_RIGHT: return "HORZ";
      case GamekInput.BUTTON_UP | GamekInput.BUTTON_DOWN: return "VERT";
      case 0: return "";
    }
    //TODO button aliases
    return btnid.toString();
  }
  
  evalButton(src) {
    src = src.trim().toUpperCase();
    if (!src) return 0;
    if (src === "HORZ") return GamekInput.BUTTON_LEFT | GamekInput.BUTTON_RIGHT;
    if (src === "VERT") return GamekInput.BUTTON_UP | GamekInput.BUTTON_DOWN;
    const asDefined = GamekInput[`BUTTON_${src}`];
    if (asDefined) return asDefined;
    //TODO button aliases
    const plainInteger = +src;
    if (plainInteger && (plainInteger >= 1) && (plainInteger < 0x10000)) {
      return Math.floor(plainInteger);
    }
    return 0;
  }
  
  isMappableOneBitButton(btnid) {
    btnid = ~~btnid;
    const okBits = 0xffff & ~GamekInput.BUTTON_CD;
    if (!(btnid & okBits)) return false;
    while (!(btnid & 1)) btnid >>= 1;
    return (btnid === 1);
  }
  
  playeridForNewDevice() {
    return 1;//TODO
  }
  
  /* Keyboard.
   ***************************************************************/
   
  getKeyboardConfiguration() {
    const config = [];
    for (const keyCode of Object.keys(this.keyMap)) {
      const map = this.keyMap[keyCode];
      config.push({ key: keyCode, player: map[0], button: this.reprButton(map[1]) });
    }
    return config;
  }
  
  setKeyboardConfiguration(config) {
    if (config) {
      this.keyMap = {};
      for (const {key, player, button} of config) {
        if (!key || (typeof(key) !== "string")) continue;
        if ((player < 0) || (player > 255) || (typeof(player) !== "number")) continue;
        if (!button || (typeof(button) !== "string")) continue;
        const btnid = this.evalButton(button);
        if (!btnid) continue;
        this.keyMap[key] = [player, this.evalButton(button)];
      }
    } else {
      this.generateDefaultKeyMap();
    }
  }
  
  generateDefaultKeyMap() {
    this.keyMap = {
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
  }
  
  onKey(event) {
    if (event.repeat) return;
    const map = this.keyMap[event.code];
    if (!map) return;
    const v = (event.type === "keydown") ? 1 : 0;
    this.setPlayerButton(map[0], map[1], v);
    event.preventDefault();
  }
  
  /* Gamepad.
   ********************************************************************/
   
  getGamepadConfiguration() {
    const config = [];
    for (const { id, axes, buttons } of this.gamepadTemplates) {
      config.push({
        id,
        axes: axes.map(btnid => this.reprButton(btnid)),
        buttons: buttons.map(btnid => this.reprButton(btnid)),
      });
    }
    return config;
  }
  
  setGamepadConfiguration(config) {
    this.gamepadTemplates = [];
    if (config) {
      for (let { id, axes, buttons } of config) {
        if (!id || (typeof(id) !== "string")) continue;
        if (!axes) axes = [];
        if (!buttons) buttons = [];
        const tm = {
          id,
          axes: axes.map(btnid => this.evalButton(btnid)),
          buttons: buttons.map(btnid => this.evalButton(btnid)),
        };
        this.gamepadTemplates.push(tm);
      }
    }
    // Now that we've rebuilt the templates, reapply them. Simulate reconnecting each device.
    const gamepads = navigator.getGamepads();
    for (const gamepad of gamepads) {
      if (!gamepad) continue;
      this.onGamepadConnection({ type: "gamepaddisconnected", gamepad });
      this.onGamepadConnection({ type: "gamepadconnected", gamepad });
    }
  }
   
  updateGamepads(gamepads) {
    for (let i=this.gamepadMaps.length; i-->0; ) {
      const map = this.gamepadMaps[i];
      const gamepad = gamepads[map[0]];
      if (!gamepad) {
        this.gamepadMaps.splice(i, 1);
        if (map[5]) { // was on, drop it
          this.setPlayerButton(map[3], map[4], 0);
        }
        continue;
      }
      switch (map[1]) {
        case "button": {
            const v = gamepad.buttons[map[2]].pressed ? 1 : 0;
            if (v !== map[5]) {
              map[5] = v;
              this.setPlayerButton(map[3], map[4], v);
            }
          } break;
        case "axislo": {
            const v = gamepad.axes[map[2]];
            if (v <= -0.1) {//TODO axis thresholds
              if (map[5]) continue;
              map[5] = 1;
              this.setPlayerButton(map[3], map[4], 1);
            } else if (map[5]) {
              map[5] = 0;
              this.setPlayerButton(map[3], map[4], 0);
            }
          } break;
        case "axis": // "axis" and "axishi" are probably the same thing. Might need different thresholds.
        case "axishi": {
            const v = gamepad.axes[map[2]];
            if (v >= 0.1) {//TODO axis thresholds
              if (map[5]) continue;
              map[5] = 1;
              this.setPlayerButton(map[3], map[4], 1);
            } else if (map[5]) {
              map[5] = 0;
              this.setPlayerButton(map[3], map[4], 0);
            }
          } break;
      }
    }
  }
  
  onGamepadConnection(event) {
    if (event.type === "gamepadconnected") {
      this.gamepads.push(event.gamepad.id);
      this.addGamepadMaps(event.gamepad);
    } else {
      this.dropGamepadState(event.gamepad);
      const p = this.gamepads.indexOf(event.gamepad.id);
      if (p >= 0) {
        this.gamepads.splice(p, 1);
      }
    }
  }
  
  dropGamepadState(gamepad) {
    const id = gamepad.id;
    console.log(`dropGamepadState ${id}`);
    for (let i=this.gamepadMaps.length; i-->0; ) {
      const map = this.gamepadMaps[i];
      if (map[0] !== id) continue;
      this.gamepadMaps.splice(i, 1);
      if (map[5]) { // was on, drop it
        this.setPlayerButton(map[3], map[4], 0);
      }
    }
  }
  
  addGamepadMaps(gamepad) {
    const playerid = this.playeridForNewDevice();
    
    // Find or synthesize template.
    let tm = this.gamepadTemplates.find(t => t.id === gamepad.id);
    if (!tm) {
      tm = this.synthesizeGamepadTemplate(gamepad);
      this.gamepadTemplates.push(tm);
    }
    
    // Apply template.
    for (let axisix=0; axisix<tm.axes.length; axisix++) {
      const btnid = tm.axes[axisix];
      if (!btnid) continue;
      switch (btnid) {
        case GamekInput.BUTTON_LEFT | GamekInput.BUTTON_RIGHT: {
            this.gamepadMaps.push([ gamepad.index, "axislo", axisix, playerid, GamekInput.BUTTON_LEFT, 0 ]);
            this.gamepadMaps.push([ gamepad.index, "axishi", axisix, playerid, GamekInput.BUTTON_RIGHT, 0 ]);
          } break;
        case GamekInput.BUTTON_UP | GamekInput.BUTTON_DOWN: {
            this.gamepadMaps.push([ gamepad.index, "axislo", axisix, playerid, GamekInput.BUTTON_UP, 0 ]);
            this.gamepadMaps.push([ gamepad.index, "axishi", axisix, playerid, GamekInput.BUTTON_DOWN, 0 ]);
          } break;
        default: {
            if (!this.isMappableOneBitButton(btnid)) continue;
            this.gamepadMaps.push([ gamepad.index, "axis", axisix, playerid, btnid, 0 ]);
          }
      }
    }
    for (let btnix=0; btnix<tm.buttons.length; btnix++) {
      const btnid = tm.buttons[btnix];
      if (!this.isMappableOneBitButton(btnid)) continue;
      this.gamepadMaps.push([ gamepad.index, "button", btnix, playerid, btnid, 0 ]);
    }
  }
  
  synthesizeGamepadTemplate(gamepad) {
    const tm = { id: gamepad.id, axes: [], buttons: [] };
    
    // If there's at least two axes, map them as X and Y.
    if (gamepad.axes.length >= 2) {
      tm.axes.push(GamekInput.BUTTON_LEFT | GamekInput.BUTTON_RIGHT);
      tm.axes.push(GamekInput.BUTTON_UP | GamekInput.BUTTON_DOWN);
    }
    while (tm.axes.length < gamepad.axes.length) tm.axes.push(0);
    
    // Map every button willy-nilly.
    const btnidv = [
      GamekInput.BUTTON_SOUTH,
      GamekInput.BUTTON_WEST,
      GamekInput.BUTTON_EAST,
      GamekInput.BUTTON_NORTH,
      GamekInput.BUTTON_AUX1,
      GamekInput.BUTTON_AUX2,
      GamekInput.BUTTON_AUX3,
      GamekInput.BUTTON_L1,
      GamekInput.BUTTON_R1,
      GamekInput.BUTTON_L2,
      GamekInput.BUTTON_R2,
    ];
    let btnidp = 0;
    for (let srcbtnid=0; srcbtnid<gamepad.buttons.length; srcbtnid++) {
      tm.buttons.push(btnidv[btnidp++]);
      if (btnidp >= btnidv.length) btnidp = 0;
    }
    
    return tm;
  }
  
  /* Touch.
   ******************************************************/
   
  getTouchConfiguration() {
    return null; //TODO
  }
  
  setTouchConfiguration(config) {
    //TODO
  }
  
  onTouch(event) {
    event.preventDefault();
    event.stopPropagation();
    
    const unprocessed = this.touches.map((v, i) => [v.identifier, i]);
    for (const src of event.touches) {
      const p = unprocessed.findIndex(u => u[0] === src.identifier);
      if (p < 0) {
      
        // Start...
        //document.querySelector(".xxxlog").innerText = `${src.clientX},${src.clientY} in ${document.body.clientWidth},${document.body.clientHeight}`;
        let btnid = this.btnidForTouchStartPosition(src.clientX, src.clientY, window.innerWidth, window.innerHeight);
        this.touches.push({
          identifier: src.identifier,
          startX: src.clientX,
          startY: src.clientY,
          btnid,
          hat: null,
          changed: false,
        });
        if (btnid && (typeof(btnid) === "number")) {
          this.setPlayerButton(1, btnid, 1);
        }
      
      } else {
        const touch = this.touches[unprocessed[p][1]];
        unprocessed.splice(p, 1);
        // Moved...
        if (touch.btnid === "dpad") {
          const dx = src.clientX - touch.startX;
          const dy = src.clientY - touch.startY;
          const hat = this.getHatDirection(dx, dy);
          if (hat !== touch.hat) {
            this.sendEventsForHatChange(1, touch.hat, hat);
            touch.hat = hat;
            touch.changed = true;
          }
        }
      
      }
    }
    if (unprocessed.length) {
      unprocessed.sort((a, b) => b.index - a.index); // important to remove higher index first
      for (const [identifier, index] of unprocessed) {
        const touch = this.touches[index];
        this.touches.splice(index, 1);
        
        // End...
        if (typeof(touch.btnid) === "number") {
          this.setPlayerButton(1, touch.btnid, 0);
          this.unchangedDpadCount = 0;
        } else if (touch.btnid === "dpad") {
          this.setPlayerButton(1, GamekInput.BUTTON_LEFT, 0);
          this.setPlayerButton(1, GamekInput.BUTTON_RIGHT, 0);
          this.setPlayerButton(1, GamekInput.BUTTON_UP, 0);
          this.setPlayerButton(1, GamekInput.BUTTON_DOWN, 0);
          if (!touch.changed) {
            if (this.unchangedDpadTime < Date.now() - 2000) { // TODO configurable
              this.unchangedDpadCount = 0;
              this.unchangedDpadTime = Date.now();
            }
            this.unchangedDpadCount++;
            if (this.unchangedDpadCount >= 3) { // TODO configurable
              this.unchangedDpadCount = 0;
              this.onTripleTap();
            }
          } else {
            this.unchangedDpadCount = 0;
          }
        }
      }
    }
  }
  
  // If the user engages the dpad three times in a row without actually moving it,
  // take that as a signal to disable touch events.
  onTripleTap() {
    if (this.touchEventListener) {
      window.removeEventListener("touchstart", this.touchEventListener, { passive: false });
      window.removeEventListener("touchend", this.touchEventListener, { passive: false });
      window.removeEventListener("touchcancel", this.touchEventListener, { passive: false });
      window.removeEventListener("touchmove", this.touchEventListener, { passive: false });
      this.touchEventListener = null;
    }
    this.onKillTouchEvents();
  }
  
  sendEventsForHatChange(playerid, prev, next) {
    const prevx = this.hatHorz(prev);
    const prevy = this.hatVert(prev);
    const nextx = this.hatHorz(next);
    const nexty = this.hatVert(next);
    if (prevy !== nexty) {
           if (prevy < 0) this.setPlayerButton(playerid, GamekInput.BUTTON_UP, 0);
      else if (prevy > 0) this.setPlayerButton(playerid, GamekInput.BUTTON_DOWN, 0);
           if (nexty < 0) this.setPlayerButton(playerid, GamekInput.BUTTON_UP, 1);
      else if (nexty > 0) this.setPlayerButton(playerid, GamekInput.BUTTON_DOWN, 1);
    }
    if (prevx !== nextx) {
           if (prevx < 0) this.setPlayerButton(playerid, GamekInput.BUTTON_LEFT, 0);
      else if (prevx > 0) this.setPlayerButton(playerid, GamekInput.BUTTON_RIGHT, 0);
           if (nextx < 0) this.setPlayerButton(playerid, GamekInput.BUTTON_LEFT, 1);
      else if (nextx > 0) this.setPlayerButton(playerid, GamekInput.BUTTON_RIGHT, 1);
    }
  }
  
  hatHorz(hat) {
    switch (hat) {
      case 7: case 6: case 5: return -1;
      case 1: case 2: case 3: return 1;
    }
    return 0;
  }
  
  hatVert(hat) {
    switch (hat) {
      case 7: case 0: case 1: return -1;
      case 5: case 4: case 3: return 1;
    }
    return 0;
  }
  
  // For a relative position in document.body.client, return 0..7 clockwise from North, or null for none.
  getHatDirection(dx, dy) {
    const limit = Math.min(window.innerWidth, window.innerHeight);
    const deadRadius = 0.050;//TODO configurable?
    dx /= limit;
    dy /= limit;
    if (dx * dx + dy * dy < deadRadius * deadRadius) return null;
    const diagonalization = 2.0;//TODO configurable? 2.0 is balanced. Higher means more diagonals, lower more cardinals.
    const adx = Math.abs(dx);
    const ady = Math.abs(dy);
    if (adx > ady * diagonalization) { // left or right
      return (dx < 0) ? 6 : 2;
    } else if (ady > adx * diagonalization) { // up or down
      return (dy < 0) ? 0 : 4;
    } else { // diagonal
      if (dx < 0) {
        return (dy < 0) ? 7 : 5;
      } else {
        return (dy < 0) ? 1 : 3;
      }
    }
  }
  
  btnidForTouchStartPosition(x, y, w, h) {
    const normx = x / w;
    const normy = y / h;
    
    if (normx < 0.500) { // Left half = dpad
      return "dpad";
      
    } else { // right half = thumb buttons
      const subx = (normx - 0.500) * 2.000;
      const suby = normy;
      if (subx > suby) {
        if (1 - subx > suby) return GamekInput.BUTTON_NORTH;
        return GamekInput.BUTTON_EAST;
      } else {
        if (subx > 1 - suby) return GamekInput.BUTTON_SOUTH;
        return GamekInput.BUTTON_WEST;
      }
    }
  }
  
  /* MIDI.
   ***********************************************************/
   
  getMidiConfiguration() {
    return null;//TODO
  }
  
  setMidiConfiguration(config) {
    //TODO
  }
  
  initMidi() {
    navigator.requestMIDIAccess().then((access) => {
      this.midiAccess = access;
      this.midiAccess.onstatechange = (event) => this.onMidiStateChange(event);
      for (const [id, input] of this.midiAccess.inputs) this.onMidiStateChange({ port: input });
      // We don't care about outputs:
      //for (const [id, output] of this.midiAccess.outputs) this.onMidiStateChange({ port: output });
    }).catch(e => console.log(`MIDI access denied`, e));
  }
  
  onMidiStateChange(event) {
    if (!event.port) return;
    if (event.port.state === "connected") {
      if (event.port.type === "input") {
        //console.log(`New MIDI input: ${event.port.manufacturer} ${event.port.name}`, event.port);
        event.port.onmidimessage = (message) => this.onMidiMessage(message);
      } else if (event.port.type === "output") {
        //console.log(`New MIDI output: ${event.port.manufacturer} ${event.port.name}`);
      }
    } else {
      //console.log(`Lost MIDI device: ${event.port.manufacturer} ${event.port.name}`);
      this.controller.onMidi([0xff]);
    }
  }
  
  onMidiMessage(message) {
    if (!this.running) return;
    // MDN says (message.data) will only contain a single MIDI message, and quick experimentation agrees. Great!
    this.controller.onMidi(message.data);
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
