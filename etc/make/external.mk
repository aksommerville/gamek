# external.mk
# Client projects should include this Makefile, and we take care of the rest.
# Gamek must already be built. We will not rebuild it.

all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $(word 2,$(subst /, ,$@)) $(@F)" ; mkdir -p $(@D) ;

SRCFILES:=$(shell find src -type f)
CFILES:=$(filter %.c %.cpp %.m %.S %.s,$(SRCFILES))
DATAFILES:=$(filter src/data/%,$(SRCFILES))

GAMEK_TARGETS:=$(filter-out tool,$(notdir $(wildcard $(GAMEK_ROOT)/out/*)))

ifeq (,$(strip $(GAMEK_TARGETS)))
  $(warning No gamek targets. Either gamek was not built, or it didn't name any targets in etc/config.mk. Try 'make gamek')
endif

define TARGET_RULES
  MIDDIR_$1:=mid/$1
  OUTDIR_$1:=out/$1
  include $(GAMEK_ROOT)/out/$1/config.mk
  CFILES_$1:=$(CFILES) $$(patsubst src/%,$$(MIDDIR_$1)/%.c,$(DATAFILES))
  OFILES_$1:=$$(patsubst src/%,$$(MIDDIR_$1)/%,$(addsuffix .o,$(basename $(CFILES))))
  EXE_$1:=$$(OUTDIR_$1)/$(PROJ_NAME)
  all:$$(EXE_$1)
  $$(EXE_$1):$$(OFILES_$1);$$(PRECMD) $$($1_LD) -o$$@ $$^ $$($1_LDPOST)
  $$(MIDDIR_$1)/%.o:          src/%.c;$$(PRECMD) $$($1_CC) -I$(GAMEK_ROOT)/out/$1/include -o$$@ $$<
  $$(MIDDIR_$1)/%.o:$$(MIDDIR_$1)/%.c;$$(PRECMD) $$($1_CC) -I$(GAMEK_ROOT)/out/$1/include -o$$@ $$<
  $$(MIDDIR_$1)/%.c:src/%;$$(PRECMD) $(GAMEK_ROOT)/out/tool/mkdata -o$$@ -i$$<
  $1-run:$$(EXE_$1);$$(EXE_$1)
endef

$(foreach T,$(GAMEK_TARGETS),$(eval $(call TARGET_RULES,$T)))
