/* SongPlayer.js
 * Understand and unpack our song format in real time.
 * A delegate is required: {
 *   config(chid, k, v)
 *   note(chid, noteid, velocity, durationms)
 *   noteOn(chid, noteid, velocity)
 *   noteOff(chid, noteid)
 * }
 * A new SongPlayer is initially suspended, you must resume() it.
 */
 
export class SongPlayer {
  constructor(window, serial, delegate) {
    this.window = window;
    
    if (!serial || !delegate || !delegate.config || !delegate.note || !delegate.noteOn || !delegate.noteOff) {
      throw new Error(`Invalid serial or delegate for SongPlayer`);
    }
    this.serial = serial;
    this.delegate = delegate;
    this.interval = null;
    this.delay = 0;
    this.heldNotes = []; // [chid, noteid]
    
    this.tempo = this.serial[0]; // ms/tick
    this.songPosition = this.serial[1];
    this.loopPosition = (this.serial[2] << 8) | this.serial[3];
    if (!this.tempo || (this.songPosition < 4) || (this.loopPosition < this.songPosition)) {
      throw new Error(`malformed song, tempo=${this.tempo} startp=${this.songPosition} loopp=${this.loopPosition}`);
    }
  }
  
  /* Public entry points.
   ************************************************************************/
  
  suspend() {
    if (this.interval === null) return;
    this.window.clearInterval(this.interval);
    this.interval = null;
    this._releaseAll();
  }
  
  resume() {
    if (this.interval !== null) return;
    this.interval = this.window.setInterval(() => this._update(), this.tempo);
  }
  
  /* Private.
   *****************************************************************************/
   
  _update() {
    if (this.delay > 0) {
      this.delay--;
      return;
    }
    while (!this.delay) {
      if (this.songPosition >= this.serial.length) {
        // This shouldn't happen, there should be an explicit EOF. But we also implicitly loop at end.
        return this._eof();
      }

      const lead = this.serial[this.songPosition];
      
      // 0: EOF (repeat)
      if (!lead) return this._eof();
      
      // High bit unset: Delay.
      if (!(lead & 0x80)) {
        this.songPosition++;
        this.delay = lead - 1; // -1: we get one tick implicitly, just by returning
        return;
      }
      
      // Note?
      if (((lead & 0xe0) === 0x80) && (this.songPosition <= this.serial.length - 3)) {
        const velocity = (lead & 0x1f) << 2;
        const noteid = (this.serial[this.songPosition + 1] >> 2) + 0x20;
        let duration = ((this.serial[this.songPosition + 1] & 3) << 4) | (this.serial[this.songPosition + 2] >> 4);
        const chid = this.serial[this.songPosition + 2] & 15;
        this.songPosition += 3;
        this.delegate.note(chid, noteid, velocity, duration * this.tempo);
        
      // Config?
      } else if (((lead & 0xf0) === 0xa0) && (this.songPosition <= this.serial.length - 3)) {
        const chid = lead & 0x0f;
        const k = this.serial[this.songPosition + 1];
        const v = this.serial[this.songPosition + 2];
        this.songPosition += 3;
        this.delegate.config(chid, k, v);
        
      // Note On?
      } else if (((lead & 0xf0) === 0xb0) && (this.songPosition <= this.serial.length - 3)) {
        const chid = lead & 0x0f;
        const noteid = this.serial[this.songPosition + 1];
        const velocity = this.serial[this.songPosition + 2];
        this.songPosition += 3;
        this.delegate.noteOn(chid, noteid, velocity);
        this._addOne(chid, noteid);
        
      // Note Off?
      } else if (((lead & 0xf0) === 0xc0) && (this.songPosition <= this.serial.length - 2)) {
        const chid = lead & 0x0f;
        const noteid = this.serial[this.songPosition + 1];
        this.songPosition += 2;
        this.delegate.noteOff(chid, noteid);
        this._releaseOne(chid, noteid);
        
      // Anything else is illegal. Die.
      } else {
        console.log(`illegal byte ${lead} at ${this.songPosition}/${this.serial.length} in song`);
        this.serial = [];
        this.suspend();
        return;
      }
    }
  }
  
  _eof() {
    this.delay = 1;
    this.songPosition = this.loopPosition;
    this._releaseAll();
  }
  
  _releaseAll() {
    for (const [chid, noteid] of this.heldNotes) {
      this.delegate.noteOff(chid, noteid);
    }
    this.heldNotes = [];
  }
  
  _releaseOne(chid, noteid) {
    for (let i=this.heldNotes.length; i-->0; ) {
      if ((this.heldNotes[i][0] === chid) && (this.heldNotes[i][1] === noteid)) {
        this.heldNotes.splice(i, 1);
      }
    }
  }
  
  _addOne(chid, noteid) {
    this.heldNotes.push([chid, noteid]);
  }
}
