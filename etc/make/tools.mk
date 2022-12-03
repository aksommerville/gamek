# tools.mk
# Rules for building helper programs that we use during the rest of the build process.

MIDDIR:=mid/tool
OUTDIR:=out/tool

# All tools get the same optional units. No big deal to include something one tool doesn't need.
TOOL_OPT_ENABLE:=argv fs serial png

# Compiler, etc. This part sometimes needs tweaking.
TOOL_CCOPT:=-c -MMD -O3
TOOL_CCDEF:= \
  $(patsubst %,-DGAMEK_USE_%=1,$(TOOL_OPT_ENABLE))
TOOL_CCINC:=-Isrc -I$(MIDDIR)
TOOL_CCWARN:=-Werror -Wimplicit
TOOL_CC:=gcc $(TOOL_CCOPT) $(TOOL_CCDEF) $(TOOL_CCINC) $(TOOL_CCWARN)
TOOL_LD:=gcc
TOOL_LDPOST:=-lz

TOOL_SRCFILES:=$(filter src/tool/% $(foreach U,$(TOOL_OPT_ENABLE),src/opt/$U/%),$(SRCFILES))
  
TOOL_CFILES:=$(filter %.c,$(TOOL_SRCFILES))
TOOL_OFILES_ALL:=$(patsubst src/%,$(MIDDIR)/%,$(addsuffix .o,$(basename $(TOOL_CFILES))))
TOOL_OFILES_COMMON:=$(filter-out $(MIDDIR)/tool/%,$(TOOL_OFILES_ALL)) $(filter $(MIDDIR)/tool/common/%,$(TOOL_OFILES_ALL))
-include $(TOOL_OFILES_ALL:.o=.d)
$(MIDDIR)/%.o:src/%.c      ;$(PRECMD) $(TOOL_CC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(TOOL_CC) -o$@ $<

define TOOL_RULES # $1=name
  TOOL_$1_EXE:=$(OUTDIR)/$1
  all:$$(TOOL_$1_EXE)
  TOOL_$1_OFILES:=$(TOOL_OFILES_COMMON) $(filter $(MIDDIR)/tool/$1/%,$(TOOL_OFILES_ALL))
  $$(TOOL_$1_EXE):$$(TOOL_$1_OFILES);$$(PRECMD) $(TOOL_LD) -o$$@ $$^ $(TOOL_LDPOST)
endef
$(foreach T,$(TOOLS),$(eval $(call TOOL_RULES,$T)))
