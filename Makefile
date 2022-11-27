all:
.SILENT:
PRECMD=echo "  $(word 2,$(subst /, ,$@)) $(@F)" ; mkdir -p $(@D) ;

ifeq ($(MAKECMDGOALS),clean)
  clean:;rm -rf mid out
else

include etc/config.mk
include etc/make/common.mk
$(foreach T,$(GAMEK_TARGETS), \
  $(eval MIDDIR:=mid/$T) \
  $(eval OUTDIR:=out/$T) \
  $(eval include etc/target/$T.mk) \
)

endif
