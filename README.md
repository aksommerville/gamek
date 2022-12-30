# gamek

Framework for low-performance video games.

I'm trying to design such that games can be built for Arduino, in particular the TinyCircuits Tiny Arcade.
So the framework aims for a very small memory footprint, and minimal CPU waste.

Documentation is all in this repo too; see [/etc/doc](https://github.com/aksommerville/gamek/tree/master/etc/doc).

## TODO

- [ ] Good quality synthesizer for PCs.
- [ ] inmgr midi: Override channel ID per device. Or maybe not? Not sure I want this.
- [ ] Other platforms
- - [ ] picosystem
- - [ ] web
- - - [ ] audio
- - - [ ] Input config only really works while the VM is running. Can we separate the input manager such that it runs at all times?
- - [ ] macos
- - - [ ] MIDI In (stubbed at opt/macaudio)
- - - [ ] Occasional noise when a sound effect starts.
- - - [ ] Allow startup without audio or hid.
- - [ ] mswin
- - [ ] thumby
- [ ] Network?
- [ ] Helper to spawn a new project.
- [ ] Standard demos
- - [ ] snake: multiplayer
- - [ ] platformer: multiplayer, high resolution
- - [ ] adventure: save progress
- - [ ] rhythm: midi input (but also joysticks)
- [ ] Build Wasm from Macos. Getting this error on my MacBook (10.13.6). Tried Wasi 17,16,15,12
dyld: lazy symbol binding failed: Symbol not found: ____chkstk_darwin
  Referenced from: /Users/andy/proj/thirdparty/wasi-sdk-15.0/bin/clang (which was built for Mac OS X 11.6)
  Expected in: /usr/lib/libSystem.B.dylib
