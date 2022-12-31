# tiny.mk
# I composed these rules by running arduino-builder and scraping it.

THIS_MAKEFILE:=$(lastword $(MAKEFILE_LIST))

tiny_OPT_ENABLE:=mynth

tiny_TOOLCHAIN:=$(tiny_ARDUINO_HOME)/packages/arduino/tools/arm-none-eabi-gcc/$(tiny_GCC_VERSION)

# -Wp,-w disables warnings about redefined macros, which we need because digitalPinToInterrupt gets defined twice in the TinyCircuits headers.
tiny_CCOPT:=-mcpu=cortex-m0plus -mthumb -c -g -Os -MMD -std=gnu11 -ffunction-sections -fdata-sections \
  -nostdlib --param max-inline-insns-single=500 -Wp,-w
tiny_CXXOPT:=$(filter-out -std=gnu11,$(tiny_CCOPT)) -std=gnu++11  -fno-threadsafe-statics -fno-rtti -fno-exceptions
tiny_CCWARN:=-Wno-expansion-to-defined -Wno-redundant-decls
tiny_CCDEF:=-DF_CPU=48000000L -DARDUINO=10819 -DARDUINO_SAMD_ZERO -DARDUINO_ARCH_SAMD -D__SAMD21G18A__ -DUSB_VID=0x03EB -DUSB_PID=0x8009 \
  -DUSBCON "-DUSB_MANUFACTURER=\"TinyCircuits\"" "-DUSB_PRODUCT=\"TinyArcade\"" -DCRYSTALLESS \
  -DGAMEK_PLATFORM_HEADER='"pf/tiny/gamek_pf_extra.h"' \
  -DGAMEK_PLATFORM_IS_TINY=1 \
  $(foreach U,$(tiny_OPT_ENABLE),-DGAMEK_USE_$U=1)
  
tiny_CCINC:=-Isrc \
  -I$(tiny_ARDUINO_HOME)/packages/arduino/tools/CMSIS/4.5.0/CMSIS/Include/ \
  -I$(tiny_ARDUINO_HOME)/packages/arduino/tools/CMSIS-Atmel/1.2.0/CMSIS/Device/ATMEL/ \
  -I$(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino \
  -I$(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/variants/tinyarcade \
  -I$(tiny_ARDUINO_IDE_HOME)/libraries/SD/src \
  -I$(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/libraries/Wire \
  -I$(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/libraries/HID \
  -I$(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/libraries/SPI 
  
tiny_CC:=$(tiny_TOOLCHAIN)/bin/arm-none-eabi-gcc $(tiny_CCOPT) $(tiny_CCWARN) $(tiny_CCDEF) $(tiny_CCINC)
tiny_CXX:=$(tiny_TOOLCHAIN)/bin/arm-none-eabi-g++ $(tiny_CXXOPT) $(tiny_CCWARN) $(tiny_CCDEF) $(tiny_CCINC)
tiny_AS:=$(tiny_TOOLCHAIN)/bin/arm-none-eabi-gcc -xassembler-with-cpp $(tiny_CCOPT) $(tiny_CCWARN) $(tiny_CCDEF) $(tiny_CCINC)

tiny_LDOPT_SOLO:=-T$(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/variants/tinyarcade/linker_scripts/gcc/flash_with_bootloader.ld 
tiny_LDOPT_HOSTED:=-T$(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/variants/tinyarcade/linker_scripts/gcc/link_for_menu.ld 
tiny_LDOPT:=-Os -Wl,--gc-sections -save-temps \
  --specs=nano.specs --specs=nosys.specs -mcpu=cortex-m0plus -mthumb \
  -Wl,--check-sections -Wl,--unresolved-symbols=report-all -Wl,--warn-common -Wl,--warn-section-align 
tiny_LDPOST:=-Wl,--start-group -L$(tiny_ARDUINO_HOME)/packages/arduino/tools/CMSIS/4.5.0/CMSIS/Lib/GCC/ -larm_cortexM0l_math -lm -Wl,--end-group
tiny_LD_SOLO:=$(tiny_TOOLCHAIN)/bin/arm-none-eabi-g++ $(tiny_LDOPT_SOLO) $(tiny_LDOPT)
tiny_LD_HOSTED:=$(tiny_TOOLCHAIN)/bin/arm-none-eabi-g++ $(tiny_LDOPT_HOSTED) $(tiny_LDOPT)

tiny_OBJCOPY:=$(tiny_TOOLCHAIN)/bin/arm-none-eabi-objcopy
tiny_AR:=$(tiny_TOOLCHAIN)/bin/arm-none-eabi-ar rc

#TODO Are any of these unnecessary?
tiny_EXTFILES:= \
  $(tiny_ARDUINO_IDE_HOME)/libraries/SD/src/File.cpp \
  $(tiny_ARDUINO_IDE_HOME)/libraries/SD/src/SD.cpp \
  $(tiny_ARDUINO_IDE_HOME)/libraries/SD/src/utility/Sd2Card.cpp \
  $(tiny_ARDUINO_IDE_HOME)/libraries/SD/src/utility/SdFile.cpp \
  $(tiny_ARDUINO_IDE_HOME)/libraries/SD/src/utility/SdVolume.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/libraries/Wire/Wire.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/libraries/HID/HID.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/libraries/SPI/SPI.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/variants/tinyarcade/variant.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/pulse_asm.S \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/USB/samd21_host.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/WInterrupts.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/avr/dtostrf.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/cortex_handlers.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/hooks.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/itoa.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/delay.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/pulse.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/startup.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/wiring.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/wiring_analog.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/wiring_shift.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/wiring_digital.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/wiring_private.c \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/IPAddress.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/Print.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/Reset.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/SERCOM.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/Stream.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/Tone.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/USB/CDC.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/USB/PluggableUSB.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/USB/USBCore.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/Uart.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/WMath.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/WString.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/abi.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/main.cpp \
  $(tiny_ARDUINO_HOME)/packages/TinyCircuits/hardware/samd/1.1.0/cores/arduino/new.cpp

# Now do our usual build stuff...
  
tiny_DATA_SRC:=$(filter src/data/% %.png %.mid %.wave,$(SRCFILES))
tiny_DATA_C:=$(patsubst src/%,$(MIDDIR)/%.c,$(tiny_DATA_SRC))
# Rules for more specific patterns could go here, eg if you need some other mkdata flag for images or whatever.
$(MIDDIR)/%.c:src/% $(TOOL_mkdata_EXE);$(PRECMD) $(TOOL_mkdata_EXE) -o$@ -i$< --imgfmt=BGR332

tiny_SRCFILES:= \
  $(filter-out src/opt/% src/test/% src/tool/% src/pf/%,$(SRCFILES)) \
  $(filter src/pf/tiny/%,$(SRCFILES)) \
  $(filter $(addsuffix /%,$(addprefix src/opt/,$(tiny_OPT_ENABLE))),$(SRCFILES)) \
  $(tiny_DATA_C)

tiny_CFILES:=$(filter %.c %.cpp %.S,$(tiny_SRCFILES)) $(tiny_EXTFILES)
tiny_OFILES:=$(patsubst src/%,$(MIDDIR)/%, \
  $(patsubst $(tiny_ARDUINO_IDE_HOME)/%,$(MIDDIR)/%, \
  $(patsubst $(tiny_ARDUINO_HOME)/%,$(MIDDIR)/%, \
  $(addsuffix .o,$(basename $(tiny_CFILES))) \
)))
-include $(tiny_OFILES:.o=.d)

define tiny_SRCRULES
  $(MIDDIR)/%.o:$1/%.c  ;$$(PRECMD) $(tiny_CC) -o $$@ $$<
  $(MIDDIR)/%.o:$1/%.cpp;$$(PRECMD) $(tiny_CXX) -o $$@ $$<
  $(MIDDIR)/%.o:$1/%.S  ;$$(PRECMD) $(tiny_AS) -o $$@ $$<
endef
$(foreach S,src $(MIDDIR) $(tiny_ARDUINO_HOME) $(tiny_ARDUINO_IDE_HOME),$(eval $(call tiny_SRCRULES,$S)))

tiny_OFILES_COMMON:=$(filter-out $(MIDDIR)/demo/%,$(tiny_OFILES))

tiny_OFILES_LIBGAMEK:=$(filter-out $(MIDDIR)/data/%,$(tiny_OFILES_COMMON))
tiny_LIBGAMEK:=$(OUTDIR)/libgamek.a
all:$(tiny_LIBGAMEK)
$(tiny_LIBGAMEK):$(tiny_OFILES_LIBGAMEK);$(PRECMD) $(tiny_AR) $@ $^

#TODO Tiny has separate linker commands for Hosted vs Solo (launch via SD card or flash direct to device).
# I want to expose both for clients' consumption. How?
tiny_CONFIG_MK:=$(OUTDIR)/config.mk
all:$(tiny_CONFIG_MK)
$(tiny_CONFIG_MK):$(THIS_MAKEFILE) etc/config.mk;$(PRECMD) echo \
  "tiny_CC:=$(filter-out -DGAMEK_PLATFORM_HEADER%,$(tiny_CC))\n" \
  "tiny_LD:=$(tiny_LD_HOSTED)\n" \
  "tiny_LDPOST:=\$$(GAMEK_ROOT)/$(tiny_LIBGAMEK) $(tiny_LDPOST)\n" \
  >$@

define tiny_HEADER_RULES
  tiny_HEADER_$1_DST:=$(OUTDIR)/include/$(notdir $1)
  generic-headers:$$(tiny_HEADER_$1_DST)
  $$(tiny_HEADER_$1_DST):$1;$$(PRECMD) cp $$< $$@
endef
$(foreach F, \
  src/pf/gamek_pf.h src/common/gamek_image.h src/common/gamek_font.h \
,$(eval $(call tiny_HEADER_RULES,$F)))
all:generic-headers

define tiny_DEMO_RULES # $1=name
  tiny_DEMO_$1_HOSTED_ELF:=$(MIDDIR)/demo/$1-hosted.elf
  tiny_DEMO_$1_SOLO_ELF:=$(MIDDIR)/demo/$1-solo.elf
  tiny_DEMO_$1_HOSTED_BIN:=$(OUTDIR)/demo/$1-hosted.bin
  tiny_DEMO_$1_SOLO_BIN:=$(OUTDIR)/demo/$1-solo.bin
  tiny_OFILES_$1:=$(tiny_OFILES_COMMON) $(filter $(MIDDIR)/demo/$1/%,$(tiny_OFILES))
  $$(tiny_DEMO_$1_HOSTED_ELF):$$(tiny_OFILES_$1);$$(PRECMD) $(tiny_LD_HOSTED) -o$$@ $$^ $(tiny_LDPOST)
  $$(tiny_DEMO_$1_SOLO_ELF):$$(tiny_OFILES_$1);$$(PRECMD) $(tiny_LD_SOLO) -o$$@ $$^ $(tiny_LDPOST)
  $$(tiny_DEMO_$1_HOSTED_BIN):$$(tiny_DEMO_$1_HOSTED_ELF);$$(PRECMD) $(tiny_OBJCOPY) -O binary $$< $$@
  $$(tiny_DEMO_$1_SOLO_BIN):$$(tiny_DEMO_$1_SOLO_ELF);$$(PRECMD) $(tiny_OBJCOPY) -O binary $$< $$@
  all:$$(tiny_DEMO_$1_HOSTED_BIN) $$(tiny_DEMO_$1_SOLO_BIN)
endef
$(foreach D,$(DEMOS),$(eval $(call tiny_DEMO_RULES,$D)))

tiny-run-%:$(OUTDIR)/demo/%-solo.bin; \
  stty -F /dev/$(tiny_PORT) 1200 ; \
  sleep 2 ; \
  $(tiny_ARDUINO_HOME)/packages/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=$(tiny_PORT) -U true -i -e -w $< -R

#tiny-sdcard:$(tiny_HOSTED_BIN);etc/tool/tiny-sdcard.sh $^

tiny-menu:; \
  stty -F /dev/$(tiny_PORT) 1200 ; \
  sleep 2 ; \
  $(tiny_ARDUINO_HOME)/packages/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=$(tiny_PORT) -U true -i -e -w etc/ArcadeMenu.ino.bin -R
