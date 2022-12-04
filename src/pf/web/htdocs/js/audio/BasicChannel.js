/* BasicChannel.js
 */
 
export class BasicVoice {
  constructor(channel, freq, velocity, cbFinish, wave) {
    this.channel = channel;
    this.cbFinish = cbFinish;
    this.begin(freq, velocity, wave);
  }
  
  release() {
    const context = this.channel.audio.context;
    this.node.gain.setValueAtTime(this.node.gain.value, context.currentTime);
    this.node.gain.linearRampToValueAtTime(0, context.currentTime + this.releaseTime);
    window.setTimeout(() => this.cbFinish(), this.releaseTime * 1000 + 10);
  }
  
  begin(freq, velocity, wave) {
    const context = this.channel.audio.context;
    
    if (velocity <= 0) {
      this.attackTime = this.channel.config[3] / 1000;
      this.decayTime = this.channel.config[5] / 1000;
      this.releaseTime = (this.channel.config[7] * 8) / 1000;
      this.attackLevel = this.channel.config[4] / 255.0;
      this.sustainLevel = this.channel.config[6] / 255.0;
    } else if (velocity >= 1) {
      this.attackTime = this.channel.config[8] / 1000;
      this.decayTime = this.channel.config[10] / 1000;
      this.releaseTime = (this.channel.config[12] * 8) / 1000;
      this.attackLevel = this.channel.config[9] / 255.0;
      this.sustainLevel = this.channel.config[11] / 255.0;
    } else {
      this.attackTime = this.channel.mixConfig(3, 8, velocity) / 1000;
      this.decayTime = this.channel.mixConfig(5, 10, velocity) / 1000;
      this.releaseTime = (this.channel.mixConfig(7, 12, velocity) * 8) / 1000;
      this.attackLevel = this.channel.mixConfig(4, 9, velocity) / 255.0;
      this.sustainLevel = this.channel.mixConfig(6, 11, velocity) / 255.0;
    }
    
    const oscillator = this.generateOscillator(context, freq, velocity, wave);
    const envelope = new GainNode(context);
    envelope.gain.setValueAtTime(0, context.currentTime);
    envelope.gain.linearRampToValueAtTime(this.attackLevel, context.currentTime + this.attackTime);
    envelope.gain.linearRampToValueAtTime(this.sustainLevel, context.currentTime + this.attackTime + this.decayTime);
    oscillator.connect(envelope);
    this.node = envelope;
  }
  
  generateOscillator(context, frequency, velocity, wave) {
  
    let makeBaseOscillator;
    if (wave.harmonics) {
      makeBaseOscillator = () => {
        const periodicWave = context.createPeriodicWave(wave.harmonics, wave.harmonicsImaginary);
        return new OscillatorNode(context, { frequency, periodicWave });
      };
    } else if (wave.shape) {
      makeBaseOscillator = () => new OscillatorNode(context, { frequency, type: wave.shape });
    } else if (wave.fmRate && wave.fmDepth) {
      makeBaseOscillator = () => {
        const modulator = new OscillatorNode(context, { frequency: frequency * wave.fmRate });
        const carrier = new OscillatorNode(context, { frequency });
        const modGain = new GainNode(context);
        //modGain.gain.value = wave.fmDepth;
        //TODO envelope-driven fmDepth sounds great. Let that be part of the configuration:
        modGain.gain.setValueAtTime(0, context.currentTime);
        modGain.gain.linearRampToValueAtTime(wave.fmDepth, context.currentTime + 0.100);
        modGain.gain.linearRampToValueAtTime(wave.fmDepth * 0.5, context.currentTime + 0.200);
        modulator.connect(modGain);
        modGain.connect(carrier.frequency);
        modulator.start();
        return carrier;
      };
    } else {
      makeBaseOscillator = () => oscillator = new OscillatorNode(context, { frequency });
    }
    
    let oscillator;
    if (wave.chorusDepth) {
      const osca = makeBaseOscillator();
      const oscb = makeBaseOscillator();
      const lfoOsc = new OscillatorNode(context, { frequency: 1 });//TODO chorus LFO rate should be configurable
      const lfoGainA = new GainNode(context);
      const lfoGainB = new GainNode(context);
      lfoGainA.gain.value = wave.chorusDepth;
      lfoGainB.gain.value = -wave.chorusDepth;
      lfoOsc.connect(lfoGainA);
      lfoOsc.connect(lfoGainB);
      lfoGainA.connect(osca.frequency);
      lfoGainB.connect(oscb.frequency);
      oscillator = new GainNode(context);
      osca.connect(oscillator);
      oscb.connect(oscillator);
      osca.start();
      oscb.start();
      lfoOsc.start();
    } else {
      oscillator = makeBaseOscillator();
      oscillator.start();
    }
    
    return oscillator;
  }
}
 
export class BasicChannel {
  constructor(audio) {
    this.audio = audio;
    this.config = [
      0, // reset, ignored
      2, // Voice Type = Wave table
      0, // waveid
      
      // minimum velocity:
      10, // attack time ms
      0x30, // attack level
      20, // decay time ms
      0x10, // sustain level
      10, // release time ms*8
      
      // maximum velocity:
      5, // attack time ms
      0x60, // attack level
      12, // decay time ms
      0x30, // sustain level
      20, // release time ms*8
    ];
  }
  
  configure(k, v) {
    this.config[k] = v;
    // Setting a minimum also resets the maximum (song must set minimum first).
    if ((k >= 3) && (k <= 7)) this.config[k + 5] = v;
  }
  
  noteOn(noteid, freq, velocity, cbFinish) {
    const wave = this.audio.waves[this.config[2]];
    if (!wave) return null;
    return new BasicVoice(this, freq, velocity, cbFinish, wave);
  }
  
  mixConfig(ka, kb, bamt) {
    const aamt = 1 - bamt;
    return this.config[ka] * aamt + this.config[kb] * bamt;
  }
}
