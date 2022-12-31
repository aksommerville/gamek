#!/bin/bash

#-----------------------------------------------------
# Collect and validate parameters.

read -p "Name (C identifier): " PROJ_NAME
if ! grep -q '^[a-zA-Z_][0-9a-zA-Z_]*$' <<<"$PROJ_NAME" ; then
  echo "Invalid project name. Aborting."
  exit 1
fi

PROJ_ROOT="../$PROJ_NAME"
if [ -e "$PROJ_ROOT" ] ; then
  echo "Error: '$PROJ_ROOT' already exists."
  exit 1
fi

read -p "Display name: " DISPLAY_NAME

#--------------------------------------------------
# Commit to disk.

echo ""
echo "Ready to create project."
echo " - Directory: $PROJ_ROOT"
echo " - Display name: $DISPLAY_NAME"
echo "Last chance to abort."
read -p "Proceed? [y/N] " PROCEED
if [ "$PROCEED" != "y" ] ; then
  echo "Aborted."
  exit 1
fi

# Directories for the skeleton project.
mkdir $PROJ_ROOT $PROJ_ROOT/src $PROJ_ROOT/etc || exit 1

# New Makefile, which mostly just defers to etc/make/external.mk.
cat - >$PROJ_ROOT/Makefile <<EOF
all:
.SILENT:
.SECONDARY:

# Controls which target reacts to "make run", and that's it.
DEFAULT_TARGET:=linux

export GAMEK_ROOT:=../gamek
export PROJ_NAME:=$PROJ_NAME
export PROJ_DISPLAY_NAME:=$DISPLAY_NAME

gamek:;make -C\$(GAMEK_ROOT)
clean:;rm -rf mid out
run:;make --no-print-directory -f\$(GAMEK_ROOT)/etc/make/external.mk \$(DEFAULT_TARGET)-run
%:;make --no-print-directory -f\$(GAMEK_ROOT)/etc/make/external.mk \$*
EOF

# Skeletal main.c.
cat - >$PROJ_ROOT/src/main.c <<EOF
#include "gamek_pf.h"
#include "gamek_image.h"
#include "gamek_font.h"
#include <stdio.h>

/* Initialize app.
 * Return zero on success, or <0 if something fails.
 */

static int8_t ${PROJ_NAME}_init() {
  return 0;
}

/* Update.
 * This gets called at about 60 Hz.
 */

static void ${PROJ_NAME}_update() {
}

/* Input.
 * Gamek does not track input state for you, you need to react to each event.
 * (playerid) is zero for the aggregate state of all player inputs.
 * For a one-player-only game, that's what you should use.
 */

static void ${PROJ_NAME}_input_event(uint8_t playerid,uint16_t btnid,uint8_t value) {
  fprintf(stderr,"%s %d.0x%04x=%d\n",__func__,playerid,btnid,value);
}

/* Render.
 * If you want to keep the previous frame, do nothing and return zero.
 * Otherwise, overwrite (fb) and return nonzero.
 */

static uint8_t ${PROJ_NAME}_render(struct gamek_image *fb) {
  gamek_image_fill_rect(fb,0,0,fb->w,fb->h,gamek_image_pixel_from_rgba(fb->fmt,0x80,0x40,0x00,0xff));
  return 1;
}

/* Client definition.
 */
 
const struct gamek_client gamek_client={
  .title="${DISPLAY_NAME}",
  .fbw=96,
  .fbh=64,
  .iconrgba=0,
  .iconw=0,
  .iconh=0,
  .init=${PROJ_NAME}_init,
  .update=${PROJ_NAME}_update,
  .render=${PROJ_NAME}_render,
  .input_event=${PROJ_NAME}_input_event,
};
EOF

# Generate README.md
cat - >$PROJ_ROOT/README.md <<EOF
# $DISPLAY_NAME
EOF

# .gitignore
cat - >$PROJ_ROOT/.gitignore <<EOF
*~
~*
mid
out
.DS_Store
EOF

# Make a git repo, then run the first build.
pushd $PROJ_ROOT || exit 1
git init . || exit 1
make || exit 1
popd

echo ""
echo "Created new project at $PROJ_ROOT"
