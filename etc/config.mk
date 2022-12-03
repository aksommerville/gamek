# config.mk
# Local configuration for gamek.

#GAMEK_TARGETS:=generic linux tiny picosystem thumby web
GAMEK_TARGETS:=generic linux

# --- linux ---
# At least one of (GLX,DRM) must be enabled. Empty to disable.
# Most users will not want DRM. It's for systems without an X server, eg bespoke game consoles.
linux_USE_GLX:=1
linux_USE_DRM:=1

run:linux-run-hello
