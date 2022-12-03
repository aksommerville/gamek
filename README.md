# gamek

Framework for low-performance video games.

I'm trying to design such that games can be built for Arduino, in particular the TinyCircuits Tiny Arcade.
So the framework aims for a very small memory footprint, and minimal CPU waste.

## TODO

- [ ] Good quality synthesizer for PCs.
- [ ] Minimal synthesizer for Tiny.
- [ ] inmgr: Override channel ID per device. Or maybe not? Not sure I want this.
- [x] linuxglx: Audio setup per command line.
- [ ] Other platforms
- - [ ] tiny
- - [ ] picosystem
- - [ ] web
- - [ ] macos
- - [ ] mswin
- - [ ] thumby
- - [ ] generic (headless, dummy drivers)
- [x] Keyboard events. Should we remove them? YES, mouse too.
- [ ] Font
- [ ] Storage, new platform API.
- [ ] Network?
- [ ] Compile-time tooling for data.
- [x] hello: Show input state.
- [x] linuxglx: Rename to "linux", it's doing both video drivers now.
- [x] linuxglx: Make stdin noncanonical when akdrm in play, and read actions off it, eg Escape.
