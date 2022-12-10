# gamek

Framework for low-performance video games.

I'm trying to design such that games can be built for Arduino, in particular the TinyCircuits Tiny Arcade.
So the framework aims for a very small memory footprint, and minimal CPU waste.

## TODO

- [ ] Good quality synthesizer for PCs.
- [ ] inmgr midi: Override channel ID per device. Or maybe not? Not sure I want this.
- [ ] Other platforms
- - [ ] picosystem
- - [ ] web
- - - [x] midi input
- - - [ ] Friendly input mapping editor. Optional, don't include in Gamek core.
- - - [ ] audio
- - - [ ] When browser window loses focus, song player start skipping. Is there a DOM event for this? Pause when unfocussed.
- - [ ] macos
- - [ ] mswin
- - [ ] thumby
- [ ] Network?
- [ ] Helper to spawn a new project.
- [ ] mynth: Generate and provide wave table.
