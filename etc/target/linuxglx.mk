# Units under src/opt to include.
linuxglx_OPT_ENABLE:=argv inmgr fs serial akx11 alsapcm ossmidi evdev

# Compiler, etc. This part sometimes needs tweaking.
linuxglx_CCOPT:=-c -MMD -O3
linuxglx_CCDEF:= \
  -DGAMEK_PLATFORM_IS_linuxglx=1 \
  -DGAMEK_PLATFORM_HEADER='"pf/linuxglx/gamek_pf_extra.h"' \
  $(patsubst %,-DGAMEK_USE_%=1,$(linuxglx_OPT_ENABLE))
linuxglx_CCINC:=-Isrc -I$(MIDDIR)
linuxglx_CCWARN:=-Werror -Wimplicit
linuxglx_CC:=gcc $(linuxglx_CCOPT) $(linuxglx_CCDEF) $(linuxglx_CCINC) $(linuxglx_CCWARN)
linuxglx_LD:=gcc
linuxglx_LDPOST:=-lX11 -lGLX -lGL -lXinerama

# Digest data files.
# All data files get turned into C code and compiled like sources.
linuxglx_DATA_SRC:=$(filter src/data/%,$(SRCFILES))
linuxglx_DATA_C:=$(patsubst src/data/%,$(MIDDIR)/data/%.c,$(linuxglx_DATA_SRC))
# Rules for more specific patterns could go here, eg if you need some other mkdata flag for images or whatever.
$(MIDDIR)/data/%.c:src/data/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$<

# A "target" isn't necessarily a "platform", but usually it is.
# Most targets should begin with this:
linuxglx_SRCFILES:= \
  $(filter-out src/pf/% src/opt/% src/tool/% src/data/%,$(SRCFILES)) \
  $(filter src/pf/linuxglx/%,$(SRCFILES)) \
  $(filter $(foreach U,$(linuxglx_OPT_ENABLE),src/opt/$U/%),$(SRCFILES)) \
  $(linuxglx_DATA_C)
#  src/pf/gamek_pf.h \
  
linuxglx_CFILES:=$(filter %.c,$(linuxglx_SRCFILES))
linuxglx_OFILES_ALL:=$(patsubst src/%,$(MIDDIR)/%,$(addsuffix .o,$(basename $(linuxglx_CFILES))))
linuxglx_OFILES_COMMON:=$(filter-out $(MIDDIR)/demo/%,$(linuxglx_OFILES_ALL))
-include $(linuxglx_OFILES_ALL:.o=.d)
$(MIDDIR)/%.o:src/%.c      ;$(PRECMD) $(linuxglx_CC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(linuxglx_CC) -o$@ $<

define linuxglx_DEMO_RULES # $1=name
  linuxglx_DEMO_$1_EXE:=$(OUTDIR)/$1
  all:$$(linuxglx_DEMO_$1_EXE)
  linuxglx_DEMO_$1_OFILES:=$(linuxglx_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(linuxglx_OFILES_ALL))
  $$(linuxglx_DEMO_$1_EXE):$$(linuxglx_DEMO_$1_OFILES);$$(PRECMD) $(linuxglx_LD) -o$$@ $$^ $(linuxglx_LDPOST)
endef
$(foreach D,$(DEMOS),$(eval $(call linuxglx_DEMO_RULES,$D)))

linuxglx-run-%:$(OUTDIR)/%;$< --input=etc/input.cfg
