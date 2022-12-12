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
- - - [x] Friendly input mapping editor. Optional, don't include in Gamek core.
- - - [ ] audio
- - - [x] When browser window loses focus, song player start skipping. Is there a DOM event for this? Pause when unfocussed.
- - - - visibilitychange event and document.hidden. Fires on tab changes and minimize but not background occluded window :(
- - - - Far as I can tell, there is no way to ask the browser "are we the foreground window?"
- - - - focus/blur on window are also sensible maybe? Doing that, we are "blurred" when user interacts with browser chrome or dev tools.
- - - [ ] Input config only really works while the VM is running. Can we separate the input manager such that it runs at all times?
- - [ ] macos
- - [ ] mswin
- - [ ] thumby
- [ ] Network?
- [ ] Helper to spawn a new project.
- [ ] mynth: Generate and provide wave table.
- [ ] Standard demos
- - [x] invaders
- - [ ] snake
- - [ ] platformer
- - [ ] adventure
- - [ ] rhythm
- [x] glx: initialize window to framebuffer aspect
