# Units under src/opt to include.
#TODO Using 'mynth' for now, but eventually we'll want a fancier synth for capable platforms like MacOS.
# There is no such opt unit as "macos", but we do use it as a compile-time flag. Leave it here.
macos_OPT_ENABLE:=argv inmgr fs serial mynth macioc macwm machid macaudio macos

# Compiler, etc. This part sometimes needs tweaking.
macos_CCOPT:=-c -MMD -O3
macos_CCDEF:= \
  -DGAMEK_PLATFORM_IS_macos=1 \
  -DGAMEK_PLATFORM_HEADER='"pf/macos/gamek_pf_extra.h"' \
  $(patsubst %,-DGAMEK_USE_%=1,$(macos_OPT_ENABLE))
macos_CCINC:=-Isrc -I$(MIDDIR)
macos_CCWARN:=-Werror -Wimplicit -Wno-parentheses -Wno-comment
macos_CC:=gcc $(macos_CCOPT) $(macos_CCDEF) $(macos_CCINC) $(macos_CCWARN)
macos_OBJC:=gcc -xobjective-c $(macos_CCOPT) $(macos_CCDEF) $(macos_CCINC) $(macos_CCWARN)
macos_LD:=gcc
macos_LDPOST:=-lm -framework Cocoa -framework Quartz -framework OpenGL -framework MetalKit -framework Metal -framework CoreGraphics -framework IOKit -framework AudioUnit

macos_DATA_SRC:=$(filter src/data/% %.png %.mid,$(SRCFILES))
macos_DATA_SRC:=$(filter-out src/pf/macos/template/%,$(macos_DATA_SRC))
macos_DATA_C:=$(patsubst src/%,$(MIDDIR)/%.c,$(macos_DATA_SRC))
$(MIDDIR)/%.c:src/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$<

macos_SRCFILES:= \
  $(filter-out src/pf/% src/opt/% src/tool/% src/data/%,$(SRCFILES)) \
  $(filter src/pf/macos/%,$(SRCFILES)) \
  $(filter $(foreach U,$(macos_OPT_ENABLE),src/opt/$U/%),$(SRCFILES)) \
  $(macos_DATA_C)
  
macos_CFILES:=$(filter %.c %.m,$(macos_SRCFILES))
macos_OFILES_ALL:=$(patsubst src/%,$(MIDDIR)/%,$(addsuffix .o,$(basename $(macos_CFILES))))
macos_OFILES_COMMON:=$(filter-out $(MIDDIR)/demo/%,$(macos_OFILES_ALL))
-include $(macos_OFILES_ALL:.o=.d)
$(MIDDIR)/%.o:src/%.c      ;$(PRECMD) $(macos_CC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(macos_CC) -o$@ $<
$(MIDDIR)/%.o:src/%.m      ;$(PRECMD) $(macos_OBJC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.m;$(PRECMD) $(macos_OBJC) -o$@ $<

macos_ICONS_DIR:=src/pf/macos/template/appicon.iconset
macos_ICONS:=$(wildcard $(macos_ICONS_DIR)/*)

#TODO APP_NAME should be the pretty form, not necessarily the executable's name as we're doing.
macos_PLIST_COMMAND=sed 's/EXE_NAME/$1/g;s/BUNDLE_IDENTIFIER/$(macos_BUNDLE_PREFIX).$1/g;s/APP_NAME/$1/g;s/APP_VERSION/1.0/g;s/COPYRIGHT/$(macos_COPYRIGHT)/g;' $$< > $$@
macos_XIB_COMMAND=sed 's/APP_NAME/$1/g' $$< > $$@

define macos_DEMO_RULES # $1=name
  macos_DEMO_$1_BUNDLE:=$(OUTDIR)/$1.app
  macos_DEMO_$1_PLIST:=$$(macos_DEMO_$1_BUNDLE)/Contents/Info.plist
  macos_DEMO_$1_XIB:=$(MIDDIR)/demo/$1/Main.xib
  macos_DEMO_$1_NIB:=$$(macos_DEMO_$1_BUNDLE)/Contents/Resources/Main.nib
  macos_DEMO_$1_EXE:=$$(macos_DEMO_$1_BUNDLE)/Contents/MacOS/$1
  macos_DEMO_$1_ICON:=$$(macos_DEMO_$1_BUNDLE)/Contents/Resources/appicon.icns
  $$(macos_DEMO_$1_PLIST):src/pf/macos/template/Info.plist;$$(PRECMD) $(call macos_PLIST_COMMAND,$1)
  $$(macos_DEMO_$1_XIB):src/pf/macos/template/Main.xib;$$(PRECMD) $(call macos_XIB_COMMAND,$1)
  $$(macos_DEMO_$1_NIB):$$(macos_DEMO_$1_XIB);$$(PRECMD) ibtool --compile $$@ $$<
  $$(macos_DEMO_$1_ICON):$(macos_ICONS);$$(PRECMD) iconutil -c icns -o $$@ $(macos_ICONS_DIR)
  macos-demo-$1:$$(macos_DEMO_$1_PLIST) $$(macos_DEMO_$1_NIB) $$(macos_DEMO_$1_EXE) $$(macos_DEMO_$1_ICON)
  all:macos-demo-$1
  macos_DEMO_$1_OFILES:=$(macos_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(macos_OFILES_ALL))
  $$(macos_DEMO_$1_EXE):$$(macos_DEMO_$1_OFILES);$$(PRECMD) $(macos_LD) -o$$@ $$^ $(macos_LDPOST)
endef
$(foreach D,$(DEMOS),$(eval $(call macos_DEMO_RULES,$D)))

macos-run-%:macos-demo-%;open -W $(macos_DEMO_$*_BUNDLE) --args --reopen-tty=$$(tty) --chdir=$$(pwd) --input=etc/input.cfg
