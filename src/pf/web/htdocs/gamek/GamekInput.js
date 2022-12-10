export class GamekInput {
  constructor(controller) {
    this.controller = controller;
    this.running = false;
    this.keyEventListener = null;
    
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
