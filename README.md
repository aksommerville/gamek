# gamek

Framework for low-performance video games.

I'm trying to design such that games can be built for Arduino, in particular the TinyCircuits Tiny Arcade.
So the framework aims for a very small memory footprint, and minimal CPU waste.

## TODO

- [ ] Good quality synthesizer for PCs.
- [x] Minimal synthesizer for Tiny.
- [ ] inmgr: Override channel ID per device. Or maybe not? Not sure I want this.
- [ ] Other platforms
- - [x] tiny
- - [ ] picosystem
- - [ ] web
- - - [ ] input
- - - [ ] audio
- - - [ ] pretty up
- - [ ] macos
- - [ ] mswin
- - [ ] thumby
- - [x] generic (headless, dummy drivers)
- [x] Font
- [ ] Storage, new platform API.
- [ ] Network?
- [ ] Compile-time tooling for data.
- [ ] Helper to spawn a new project.
- [x] tiny: Prevent interference with synthesizer while update in progress.
- [ ] mynth: Generate and provide wave table.
- [ ] mynth: Songs from MIDI.
