ifeq (,$(strip $(linux_USE_GLX) $(linux_USE_DRM)))
  $(error Please set at least one of (linux_USE_GLX,linux_USE_DRM))
endif

# Units under src/opt to include.
linux_OPT_ENABLE:=argv inmgr fs serial alsapcm ossmidi evdev
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
linux_LD:=gcc
linux_LDPOST:=
ifneq (,$(strip $(linux_USE_GLX)))
  linux_LDPOST+=-lX11 -lGLX -lGL -lXinerama
endif
ifneq (,$(strip $(linux_USE_DRM)))
  linux_LDPOST+=-ldrm -lgbm -lEGL -lGL
endif

# Digest data files.
# All data files get turned into C code and compiled like sources.
linux_DATA_SRC:=$(filter src/data/%,$(SRCFILES))
linux_DATA_C:=$(patsubst src/data/%,$(MIDDIR)/data/%.c,$(linux_DATA_SRC))
# Rules for more specific patterns could go here, eg if you need some other mkdata flag for images or whatever.
$(MIDDIR)/data/%.c:src/data/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$<

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

define linux_DEMO_RULES # $1=name
  linux_DEMO_$1_EXE:=$(OUTDIR)/$1
  all:$$(linux_DEMO_$1_EXE)
  linux_DEMO_$1_OFILES:=$(linux_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(linux_OFILES_ALL))
  $$(linux_DEMO_$1_EXE):$$(linux_DEMO_$1_OFILES);$$(PRECMD) $(linux_LD) -o$$@ $$^ $(linux_LDPOST)
endef
$(foreach D,$(DEMOS),$(eval $(call linux_DEMO_RULES,$D)))

linux-run-%:$(OUTDIR)/%;$< --input=etc/input.cfg