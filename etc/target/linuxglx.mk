linuxglx_OPT_ENABLE:=argv inmgr fs serial

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

# A "target" isn't necessarily a "platform", but usually it is.
# Most targets should begin with this:
linuxglx_SRCFILES:= \
  $(filter-out src/pf/% src/opt/%,$(SRCFILES)) \
  $(filter src/pf/linuxglx/%,$(SRCFILES)) \
  $(filter $(foreach U,$(linuxglx_OPT_ENABLE),src/opt/$U/%),$(SRCFILES)) \
  src/pf/gamek_pf.h
  
#TODO Generated source files?
#TODO Data files?
  
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
