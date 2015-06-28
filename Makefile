#
# Copyright (c) 2015 Sergi Granell (xerpi)
# based on Cirne's vita-toolchain test Makefile
#

TARGET = FTPVita
OBJS   = main.o ftp.o console.o draw.o font_data.o

LIBS = -lc_stub -lc -lgcc -lSceKernel_stub -lSceDisplay_stub -lSceGxm_stub	\
	-lSceNet_stub -lSceNetCtl_stub -lSceCtrl_stub

PREFIX  = $(DEVKITARM)/bin/arm-none-eabi
CC      = $(PREFIX)-gcc
READELF = $(PREFIX)-readelf
OBJDUMP = $(PREFIX)-objdump
CFLAGS  = -Wall -specs=psp2.specs
ASFLAGS = $(CFLAGS)

CFLAGS  += -Wno-unused-but-set-variable

all: $(TARGET)_fixup.elf

debug: CFLAGS += -DSHOW_DEBUG=1
debug: all

%_fixup.elf: %.elf
	psp2-fixup -q -S $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	@rm -rf $(TARGET)_fixup.elf $(TARGET).elf $(OBJS)

copy: $(TARGET)_fixup.elf
	@cp $(TARGET)_fixup.elf ~/shared/vitasample.elf
	@echo "Copied!"

