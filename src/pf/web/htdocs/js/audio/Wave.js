/* Wave.js
 * Oscillator definitions.
 */
 
export class Wave {
  constructor() {
    this.harmonics = null; // array of 0..1
    this.harmonicsImaginary = null;
    this.shape = null; // sine square saw triangle
    this.chorusDepth = 0; // cents
    this.fmRate = 0; // multiplier against note rate
    this.fmDepth = 0;
  }
  
  /* Decode a text dump into multiple waves.
   *******************************************************************/
  
  static decodeAll(src) {
    const waves = [];
    let wave = null;
    let srcp = 0;
    while (srcp < src.length) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp);
      srcp = nlp + 1;
      const words = line.split(/\s+/);
      switch (words[0]) {
      
        case "wave": {
            const waveid = +words[1];
            if (isNaN(waveid) || (waveid < 0) || (waveid >= 8)) {
              throw new Error(`Invalid waveid ${waveid}`);
            }
            wave = new Wave();
            waves[waveid] = wave;
          } break;
          
        case "harmonics": {
            if (!wave) throw new Error(`Expected 'wave ID' before '${words[0]}' command.`);
            wave.harmonics = words.slice(1).map(n => n / 1000);
            wave.harmonicsImaginary = wave.harmonics.map(() => 0);
          } break;
          
        case "sine":
        case "sawtooth":
        case "square":
        case "triangle": {
            if (!wave) throw new Error(`Expected 'wave ID' before '${words[0]}' command.`);
            wave.shape = words[0];
          } break;
          
        case "chorus": {
            if (!wave) throw new Error(`Expected 'wave ID' before '${words[0]}' command.`);
            wave.chorusDepth = +words[1];
          } break;
          
        case "fm": {
            if (!wave) throw new Error(`Expected 'wave ID' before '${words[0]}' command.`);
            wave.fmRate = +words[1];
            wave.fmDepth = +words[2];
          } break;
          
        default: throw new Error(`Unknown wave command '${words[0]}'`);
      }
    }
    return waves;
  }
}
