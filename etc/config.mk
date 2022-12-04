# config.mk
# Local configuration for gamek.

# Which targets are we building? See etc/target for options.
GAMEK_TARGETS:=generic linux tiny web

run:linux-run-hello
serve:web-serve

# --- linux ---
# At least one of (GLX,DRM) must be enabled. Empty to disable.
# Most users will not want DRM. It's for systems without an X server, eg bespoke game consoles.
linux_USE_GLX:=1
linux_USE_DRM:=1

# --- web ---
web_WASI_SDK:=/home/andy/proj/thirdparty/wasi-sdk-16.0
web_SERVER_COMMAND:=http-server -a localhost -p 8080 -c-1

# --- tiny ---
# https://arduino.cc
tiny_ARDUINO_HOME:=/home/andy/.arduino15
tiny_ARDUINO_IDE_HOME:=/home/andy/.local/share/umake/electronics/arduino/
tiny_GCC_VERSION:=7-2017q4
tiny_PORT:=ttyACM0
