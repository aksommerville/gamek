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
- - - [x] Application bundles.
- - - [x] IoC/WM
- - - [x] HID
- - - [x] PCM Out
- - - [ ] MIDI In (stubbed at opt/macaudio)
- - - [x] Gamek platform glue.
- - - [x] Update generic icons, plist, xib (currently from Full Moon).
- - - [ ] Occasional noise when a sound effect starts.
- - - [ ] Allow startup without audio or hid.
- - - [x] macwm: Initial window size, scale up to a reasonable size.
- - [ ] mswin
- - [ ] thumby
- [ ] Network?
- [ ] Helper to spawn a new project.
- [x] mynth: Generate and provide wave table.
- [ ] Standard demos
- - [x] invaders
- - [ ] snake
- - [ ] platformer
- - [ ] adventure
- - [ ] rhythm
- [x] glx: initialize window to framebuffer aspect
- [ ] config.mk should be gitignored, with an example template
- [ ] Build Wasm from Macos. Getting this error on my MacBook (10.13.6). Tried Wasi 17,16,15,12
dyld: lazy symbol binding failed: Symbol not found: ____chkstk_darwin
  Referenced from: /Users/andy/proj/thirdparty/wasi-sdk-15.0/bin/clang (which was built for Mac OS X 11.6)
  Expected in: /usr/lib/libSystem.B.dylib
- [x] fiddle: Load synth config and monitor with inotify.
