# generic.mk
# Rules for building gamek with no I/O drivers.
# This may be useful for testing or something?
# But also as a starting point for writing new targets.

# Units under src/opt to include.
generic_OPT_ENABLE:=

# Compiler, etc. This part sometimes needs tweaking.
generic_CCOPT:=-c -MMD -O3
generic_CCDEF:= \
  $(patsubst %,-DGAMEK_USE_%=1,$(generic_OPT_ENABLE))
generic_CCINC:=-Isrc -I$(MIDDIR)
generic_CCWARN:=-Werror -Wimplicit -Wno-parentheses
generic_CC:=gcc $(generic_CCOPT) $(generic_CCDEF) $(generic_CCINC) $(generic_CCWARN)
generic_LD:=gcc
generic_LDPOST:=-lm

# Digest data files.
# All data files get turned into C code and compiled like sources.
generic_DATA_SRC:=$(filter src/data/% %.png %.mid %.wave,$(SRCFILES))
generic_DATA_C:=$(patsubst src/%,$(MIDDIR)/%.c,$(generic_DATA_SRC))
# Rules for more specific patterns could go here, eg if you need some other mkdata flag for images or whatever.
$(MIDDIR)/%.c:src/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$<

# A "target" isn't necessarily a "platform", but usually it is.
# Most targets should begin with this:
generic_SRCFILES:= \
  $(filter-out src/pf/% src/opt/% src/tool/% src/data/%,$(SRCFILES)) \
  $(filter src/pf/generic/%,$(SRCFILES)) \
  $(filter $(foreach U,$(generic_OPT_ENABLE),src/opt/$U/%),$(SRCFILES)) \
  $(generic_DATA_C)
  
generic_CFILES:=$(filter %.c,$(generic_SRCFILES))
generic_OFILES_ALL:=$(patsubst src/%,$(MIDDIR)/%,$(addsuffix .o,$(basename $(generic_CFILES))))
generic_OFILES_COMMON:=$(filter-out $(MIDDIR)/demo/%,$(generic_OFILES_ALL))
-include $(generic_OFILES_ALL:.o=.d)
$(MIDDIR)/%.o:src/%.c      ;$(PRECMD) $(generic_CC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(generic_CC) -o$@ $<

define generic_DEMO_RULES # $1=name
  generic_DEMO_$1_EXE:=$(OUTDIR)/$1
  all:$$(generic_DEMO_$1_EXE)
  generic_DEMO_$1_OFILES:=$(generic_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(generic_OFILES_ALL))
  $$(generic_DEMO_$1_EXE):$$(generic_DEMO_$1_OFILES);$$(PRECMD) $(generic_LD) -o$$@ $$^ $(generic_LDPOST)
endef
$(foreach D,$(DEMOS),$(eval $(call generic_DEMO_RULES,$D)))

generic-run-%:$(OUTDIR)/%;$< --input=etc/input.cfg
