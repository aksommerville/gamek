# gamek

Framework for low-performance video games.

I'm trying to design such that games can be built for Arduino, in particular the TinyCircuits Tiny Arcade.
So the framework aims for a very small memory footprint, and minimal CPU waste.

## TODO

- [ ] Good quality synthesizer for PCs.
- [x] Minimal synthesizer for Tiny.
- [ ] inmgr midi: Override channel ID per device. Or maybe not? Not sure I want this.
- [ ] Other platforms
- - [x] tiny
- - [ ] picosystem
- - [ ] web
- - - [x] key mapping
- - - [x] gamepad
- - - [ ] touch input
- - - [ ] midi input
- - - [ ] Friendly input mapping editor. Optional, don't include in Gamek core.
- - - [ ] audio
- - - [x] fullscreen toggle
- - - [x] pretty up
- - - [ ] When browser window loses focus, song player start skipping. Is there a DOM event for this? Pause when unfocussed.
- - [ ] macos
- - [ ] mswin
- - [ ] thumby
- - [x] generic (headless, dummy drivers)
- [x] Font
- [x] Storage, new platform API.
- [ ] Network?
- [x] Compile-time tooling for data.
- [ ] Helper to spawn a new project.
- [x] tiny: Prevent interference with synthesizer while update in progress.
- [ ] mynth: Generate and provide wave table.
- [x] mynth: Songs from MIDI.
