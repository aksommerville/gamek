# linuxdrm.mk
# This is basically the same as linuxglx.mk, and probably glx is the one you want.
# linuxdrm uses the Direct Rendering Manager, for systems that don't have a window manager.
# Yes, it would totally be possible to build them as one target.
# Come to think of it.
# Do that.

# Units under src/opt to include.
linuxdrm_OPT_ENABLE:=argv inmgr fs serial akx11 alsapcm ossmidi evdev

# Compiler, etc. This part sometimes needs tweaking.
linuxdrm_CCOPT:=-c -MMD -O3
linuxdrm_CCDEF:= \
  -DGAMEK_PLATFORM_IS_linuxdrm=1 \
  -DGAMEK_PLATFORM_HEADER='"pf/linuxdrm/gamek_pf_extra.h"' \
  $(patsubst %,-DGAMEK_USE_%=1,$(linuxdrm_OPT_ENABLE))
linuxdrm_CCINC:=-Isrc -I$(MIDDIR)
linuxdrm_CCWARN:=-Werror -Wimplicit
linuxdrm_CC:=gcc $(linuxdrm_CCOPT) $(linuxdrm_CCDEF) $(linuxdrm_CCINC) $(linuxdrm_CCWARN)
linuxdrm_LD:=gcc
linuxdrm_LDPOST:=-lX11 -lGLX -lGL -lXinerama

# Digest data files.
# All data files get turned into C code and compiled like sources.
linuxdrm_DATA_SRC:=$(filter src/data/%,$(SRCFILES))
linuxdrm_DATA_C:=$(patsubst src/data/%,$(MIDDIR)/data/%.c,$(linuxdrm_DATA_SRC))
# Rules for more specific patterns could go here, eg if you need some other mkdata flag for images or whatever.
$(MIDDIR)/data/%.c:src/data/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$<

# A "target" isn't necessarily a "platform", but usually it is.
# Most targets should begin with this:
linuxdrm_SRCFILES:= \
  $(filter-out src/pf/% src/opt/% src/tool/% src/data/%,$(SRCFILES)) \
  $(filter src/pf/linuxdrm/%,$(SRCFILES)) \
  $(filter $(foreach U,$(linuxdrm_OPT_ENABLE),src/opt/$U/%),$(SRCFILES)) \
  $(linuxdrm_DATA_C)
#  src/pf/gamek_pf.h \
  
linuxdrm_CFILES:=$(filter %.c,$(linuxdrm_SRCFILES))
linuxdrm_OFILES_ALL:=$(patsubst src/%,$(MIDDIR)/%,$(addsuffix .o,$(basename $(linuxdrm_CFILES))))
linuxdrm_OFILES_COMMON:=$(filter-out $(MIDDIR)/demo/%,$(linuxdrm_OFILES_ALL))
-include $(linuxdrm_OFILES_ALL:.o=.d)
$(MIDDIR)/%.o:src/%.c      ;$(PRECMD) $(linuxdrm_CC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(linuxdrm_CC) -o$@ $<

define linuxdrm_DEMO_RULES # $1=name
  linuxdrm_DEMO_$1_EXE:=$(OUTDIR)/$1
  all:$$(linuxdrm_DEMO_$1_EXE)
  linuxdrm_DEMO_$1_OFILES:=$(linuxdrm_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(linuxdrm_OFILES_ALL))
  $$(linuxdrm_DEMO_$1_EXE):$$(linuxdrm_DEMO_$1_OFILES);$$(PRECMD) $(linuxdrm_LD) -o$$@ $$^ $(linuxdrm_LDPOST)
endef
$(foreach D,$(DEMOS),$(eval $(call linuxdrm_DEMO_RULES,$D)))

linuxdrm-run-%:$(OUTDIR)/%;$< --input=etc/input.cfg
