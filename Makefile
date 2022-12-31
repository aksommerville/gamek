all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $(word 2,$(subst /, ,$@)) $(@F)" ; mkdir -p $(@D) ;

ifeq ($(MAKECMDGOALS),clean)
  clean:;rm -rf mid out
else

etc/config.mk:;echo "  Copying default configuration to $@" ; cp etc/config.mk.example etc/config.mk

include etc/config.mk
include etc/make/common.mk
include etc/make/tools.mk

$(foreach T,$(GAMEK_TARGETS), \
  $(eval MIDDIR:=mid/$T) \
  $(eval OUTDIR:=out/$T) \
  $(eval include etc/target/$T.mk) \
)

project:all;etc/tool/project.sh

endif
