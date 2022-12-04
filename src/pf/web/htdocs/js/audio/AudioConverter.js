/* AudioConverter.js
 * Frequency tables and primitive formatting.
 */
 
export class AudioConverter {
  static getDependencies() {
    return [];
  }
  constructor() {
  
    this.freqv = []; // hz; indexed by noteid 0..127
    
    this._initializeFrequencyTable(0x45, 440);
  }
   
  frequencyFromNoteid(noteid) {
    return this.freqv[noteid & 0x7f];
  }
  
  normalizeVelocity(sevenBit) {
    if (sevenBit <= 0) return 0;
    if (sevenBit >= 127) return 1;
    return sevenBit / 127.0;
  }
   
  _initializeFrequencyTable(refNoteid, refFreq) {
    this.freqv = [];
    for (let noteid=0; noteid<0x80; noteid++) {
      this.freqv.push(refFreq * Math.pow(2, (noteid - refNoteid) / 12));
    }
  }
}

AudioConverter.singleton = true;
