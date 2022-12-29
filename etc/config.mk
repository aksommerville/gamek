# config.mk
# Local configuration for gamek.

# Which targets are we building? See etc/target for options.
GAMEK_TARGETS:=macos

# Extra optional units for the tools.
# Most tools are generic, but fiddle requires MIDI, PCM, and synth APIs.
TOOL_OPT_EXTRA:=mynth

run:linux-run-snake
serve:web-serve
serve-dev:web-serve-dev

# To run fiddle with real waves, we have to specify each file at the command line.
# This is not ideal, but I don't have a better plan. Wave ID to file name gets associated programmatically by the game.
# Luckily, all other config (for Mynth at least) comes via MIDI events, so we don't need special handling for most things.
fiddle:tool-run-fiddle
TOOL_fiddle_ARGS:=--wave-0=src/demo/invaders/lead.wave --wave-1=src/demo/invaders/bass.wave

# --- linux ---
# At least one of (GLX,DRM) must be enabled. Empty to disable.
# Most users will not want DRM. It's for systems without an X server, eg bespoke game consoles.
linux_USE_GLX:=1
linux_USE_DRM:=1

# --- web ---
#web_WASI_SDK:=/home/andy/proj/thirdparty/wasi-sdk-16.0
web_WASI_SDK:=/Users/andy/proj/thirdparty/wasi-sdk-15.0
web_WASI_COPT:=-fno-stack-check
web_SERVER_COMMAND:=http-server -a localhost -p 8080 -c-1
web_DEV_HOST:=
web_DEV_PORT:=8080

# --- tiny ---
# https://arduino.cc
tiny_ARDUINO_HOME:=/home/andy/.arduino15
tiny_ARDUINO_IDE_HOME:=/home/andy/.local/share/umake/electronics/arduino/
tiny_GCC_VERSION:=7-2017q4
tiny_PORT:=ttyACM0
