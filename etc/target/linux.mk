THIS_MAKEFILE:=$(lastword $(MAKEFILE_LIST))

ifeq (,$(strip $(linux_USE_GLX) $(linux_USE_DRM)))
  $(error Please set at least one of (linux_USE_GLX,linux_USE_DRM))
endif

# Units under src/opt to include.
#TODO Using 'mynth' for now, but eventually we'll want a fancier synth for capable platforms like Linux.
linux_OPT_ENABLE:=argv inmgr fs serial alsapcm ossmidi evdev mynth
ifneq (,$(strip $(linux_USE_GLX)))
  linux_OPT_ENABLE+=akx11
endif
ifneq (,$(strip $(linux_USE_DRM)))
  linux_OPT_ENABLE+=akdrm
endif

# Compiler, etc. This part sometimes needs tweaking.
linux_CCOPT:=-c -MMD -O3
linux_CCDEF:= \
  -DGAMEK_PLATFORM_IS_linux=1 \
  -DGAMEK_PLATFORM_HEADER='"pf/linux/gamek_pf_extra.h"' \
  $(patsubst %,-DGAMEK_USE_%=1,$(linux_OPT_ENABLE))
linux_CCINC:=-Isrc -I$(MIDDIR)
ifneq (,$(strip $(linux_USE_DRM)))
  linux_CCINC+=-I/usr/include/libdrm
endif
linux_CCWARN:=-Werror -Wimplicit
linux_CC:=gcc $(linux_CCOPT) $(linux_CCDEF) $(linux_CCINC) $(linux_CCWARN)
linux_AR:=ar rc
linux_LD:=gcc
linux_LDPOST:=-lm
ifneq (,$(strip $(linux_USE_GLX)))
  linux_LDPOST+=-lX11 -lGLX -lGL -lXinerama
endif
ifneq (,$(strip $(linux_USE_DRM)))
  linux_LDPOST+=-ldrm -lgbm -lEGL -lGL
endif

# Digest data files.
# All data files get turned into C code and compiled like sources.
linux_DATA_SRC:=$(filter src/data/% %.png %.mid %.wave,$(SRCFILES))
linux_DATA_C:=$(patsubst src/%,$(MIDDIR)/%.c,$(linux_DATA_SRC))
# Rules for more specific patterns could go here, eg if you need some other mkdata flag for images or whatever.
$(MIDDIR)/%.c:src/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$<

# A "target" isn't necessarily a "platform", but usually it is.
# Most targets should begin with this:
linux_SRCFILES:= \
  $(filter-out src/pf/% src/opt/% src/tool/% src/data/%,$(SRCFILES)) \
  $(filter src/pf/linux/%,$(SRCFILES)) \
  $(filter $(foreach U,$(linux_OPT_ENABLE),src/opt/$U/%),$(SRCFILES)) \
  $(linux_DATA_C)
  
linux_CFILES:=$(filter %.c,$(linux_SRCFILES))
linux_OFILES_ALL:=$(patsubst src/%,$(MIDDIR)/%,$(addsuffix .o,$(basename $(linux_CFILES))))
linux_OFILES_COMMON:=$(filter-out $(MIDDIR)/demo/%,$(linux_OFILES_ALL))
-include $(linux_OFILES_ALL:.o=.d)
$(MIDDIR)/%.o:src/%.c      ;$(PRECMD) $(linux_CC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(linux_CC) -o$@ $<

linux_OFILES_LIBGAMEK:=$(filter-out $(MIDDIR)/data/%,$(linux_OFILES_COMMON))
linux_LIBGAMEK:=$(OUTDIR)/libgamek.a
all:$(linux_LIBGAMEK)
$(linux_LIBGAMEK):$(linux_OFILES_LIBGAMEK);$(PRECMD) $(linux_AR) $@ $^

linux_CONFIG_MK:=$(OUTDIR)/config.mk
all:$(linux_CONFIG_MK)
$(linux_CONFIG_MK):$(THIS_MAKEFILE) etc/config.mk;$(PRECMD) echo \
  "linux_CC:=$(filter-out -DGAMEK_PLATFORM_HEADER%,$(linux_CC))\n" \
  "linux_LD:=$(linux_LD)\n" \
  "linux_LDPOST:=\$$(GAMEK_ROOT)/$(linux_LIBGAMEK) $(linux_LDPOST)\n" \
  >$@

#TODO incorporate GAMEK_PLATFORM_HEADER somehow (macos and tiny too)
define linux_HEADER_RULES
  linux_HEADER_$1_DST:=$(OUTDIR)/include/$(notdir $1)
  generic-headers:$$(linux_HEADER_$1_DST)
  $$(linux_HEADER_$1_DST):$1;$$(PRECMD) cp $$< $$@
endef
$(foreach F, \
  src/pf/gamek_pf.h src/common/gamek_image.h src/common/gamek_font.h \
,$(eval $(call linux_HEADER_RULES,$F)))
all:generic-headers

define linux_DEMO_RULES # $1=name
  linux_DEMO_$1_EXE:=$(OUTDIR)/demo/$1
  all:$$(linux_DEMO_$1_EXE)
  linux_DEMO_$1_OFILES:=$(linux_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(linux_OFILES_ALL))
  $$(linux_DEMO_$1_EXE):$$(linux_DEMO_$1_OFILES);$$(PRECMD) $(linux_LD) -o$$@ $$^ $(linux_LDPOST)
endef
$(foreach D,$(DEMOS),$(eval $(call linux_DEMO_RULES,$D)))

linux-run-%:$(OUTDIR)/demo/%;$< --input=etc/input.cfg
