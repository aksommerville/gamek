# web.mk
# Rules for building gamek for WebAssembly.
# It being 2022, I expect this is how most players will do it.
# So get this right :)

# Units under src/opt to include.
web_OPT_ENABLE:=

# Compiler, etc. This part sometimes needs tweaking.
web_LDOPT:=-nostdlib -Xlinker --no-entry -Xlinker --import-undefined -Xlinker --export-all
web_CCOPT:=-c -MMD -O3 -nostdlib
web_CCDEF:= \
  $(patsubst %,-DGAMEK_USE_%=1,$(web_OPT_ENABLE))
web_CCINC:=-Isrc -I$(MIDDIR)
web_CCWARN:=-Werror -Wimplicit
web_CC:=$(web_WASI_SDK)/bin/clang $(web_CCOPT) $(web_CCDEF) $(web_CCINC) $(web_CCWARN)
web_LD:=$(web_WASI_SDK)/bin/clang $(web_LDOPT)
web_LDPOST:=

# Digest data files.
# All data files get turned into C code and compiled like sources.
web_DATA_SRC:=$(filter src/data/%,$(SRCFILES))
web_DATA_C:=$(patsubst src/data/%,$(MIDDIR)/data/%.c,$(web_DATA_SRC))
# Rules for more specific patterns could go here, eg if you need some other mkdata flag for images or whatever.
$(MIDDIR)/data/%.c:src/data/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$<

# A "target" isn't necessarily a "platform", but usually it is.
# Most targets should begin with this:
web_SRCFILES:= \
  $(filter-out src/pf/% src/opt/% src/tool/% src/data/%,$(SRCFILES)) \
  $(filter src/pf/web/%,$(SRCFILES)) \
  $(filter $(foreach U,$(web_OPT_ENABLE),src/opt/$U/%),$(SRCFILES)) \
  $(web_DATA_C)
  
web_CFILES:=$(filter %.c,$(web_SRCFILES))
web_OFILES_ALL:=$(patsubst src/%,$(MIDDIR)/%,$(addsuffix .o,$(basename $(web_CFILES))))
web_OFILES_COMMON:=$(filter-out $(MIDDIR)/demo/%,$(web_OFILES_ALL))
-include $(web_OFILES_ALL:.o=.d)
$(MIDDIR)/%.o:src/%.c      ;$(PRECMD) $(web_CC) -o$@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(web_CC) -o$@ $<

web_ALL_EXES:=

define web_DEMO_RULES # $1=name
  web_DEMO_$1_EXE:=$(OUTDIR)/$1.wasm
  all:$$(web_DEMO_$1_EXE)
  web_ALL_EXES+=$$(web_DEMO_$1_EXE)
  web_DEMO_$1_OFILES:=$(web_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(web_OFILES_ALL))
  $$(web_DEMO_$1_EXE):$$(web_DEMO_$1_OFILES);$$(PRECMD) $(web_LD) -o$$@ $$^ $(web_LDPOST)
endef
$(foreach D,$(DEMOS),$(eval $(call web_DEMO_RULES,$D)))

web_HTDOCS_SRC:=$(filter src/pf/web/htdocs/%,$(SRCFILES))
web_HTDOCS:=$(patsubst src/pf/web/htdocs/%,$(OUTDIR)/www/%,$(web_HTDOCS_SRC)) $(OUTDIR)/www/contents.json
all:$(web_HTDOCS)
$(OUTDIR)/www/%:src/pf/web/htdocs/%;$(PRECMD) cp $< $@
$(OUTDIR)/www/contents.json:$(web_ALL_EXES);$(PRECMD) echo '$(DEMOS)' | sed -E 's/([^ ]+)/"\1"/g;s/ /,/g;s/^.*$$/[&]/' > $@

# Demos go at the top level "out" directory, but also under "www", for convenience in serving them.
web_HTDOCS_DEMOS:=$(patsubst $(OUTDIR)/%,$(OUTDIR)/www/%,$(web_ALL_EXES))
all:$(web_HTDOCS_DEMOS)
$(OUTDIR)/www/%.wasm:$(OUTDIR)/%.wasm;$(PRECMD) cp $< $@

ifneq (,$(strip $(web_SERVER_COMMAND)))
  web-serve:$(web_HTDOCS_DEMOS) $(web_HTDOCS);$(web_SERVER_COMMAND) $(OUTDIR)/www
endif
