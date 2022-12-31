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
- [x] Helper to spawn a new project.
- [ ] Standard demos
- - [x] snake
- - [ ] platformer: multiplayer, high resolution
- - [ ] adventure: save progress
- - [ ] rhythm: midi input (but also joysticks)
- [ ] Build Wasm from Macos. Getting this error on my MacBook (10.13.6). Tried Wasi 17,16,15,12
dyld: lazy symbol binding failed: Symbol not found: ____chkstk_darwin
  Referenced from: /Users/andy/proj/thirdparty/wasi-sdk-15.0/bin/clang (which was built for Mac OS X 11.6)
  Expected in: /usr/lib/libSystem.B.dylib
- [ ] Might need to simplify Mynth more aggressively. "snakeshakin" seems to push Tiny's limits.
- [ ] Can we expose the framebuffer dimensions at init? Kind of important to know, before the first render.
- [x] evdev or inmgr: First joystick should be player 1, same as the keyboard.
- [x] evdev or inmgr: Not getting CD=0 after a disconnect.
- [x] linux: input events are firing before init! (macos is probably affected too)
- [ ] inmgr: dynamic player limit. currently hard-coded at 16
- - Also, consider reusing gap playerid. eg you disconnect player 2 then reconnect it. Today that becomes player 3 instead of reusing 2.
