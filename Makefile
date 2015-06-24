#
# Copyright (c) 2015 Sergi Granell (xerpi)
# based on Cirne's vita-toolchain test Makefile
#

TARGET = vitahelloworld
OBJS   = main.o ftp.o console.o draw.o font_data.o

STUBS = libSceLibKernel.a libSceDisplay.a libSceGxm.a libSceSysmem.a \
	libSceThreadmgr.a libSceLibc.a libSceNet.a libSceNetCtl.a \
	libSceCtrl.a

LIBS =

NIDS_DB = sample-db.json

PREFIX  = $(DEVKITARM)/bin/arm-none-eabi
CC      = $(PREFIX)-gcc
READELF = $(PREFIX)-readelf
OBJDUMP = $(PREFIX)-objdump
CFLAGS  = -Wall -Wno-unused-but-set-variable -nostartfiles -nostdlib -I$(PSP2SDK)/include

STUBS_FULL = $(addprefix stubs/, $(STUBS))

all: $(TARGET).velf

debug: CFLAGS += -DSHOW_DEBUG=1
debug: all

$(TARGET).velf: $(TARGET).elf
	vita-elf-create $(TARGET).elf $(TARGET).velf $(NIDS_DB)

$(TARGET).elf: $(OBJS) $(STUBS_FULL)
	$(CC) -Wl,-q -o $@ $^ $(LIBS) $(CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(STUBS_FULL):
	@mkdir -p stubs
	vita-libs-gen $(NIDS_DB) stubs/
	$(MAKE) -C stubs $(notdir $@)

clean:
	@rm -rf $(TARGET).elf $(TARGET).velf $(OBJS) stubs

copy: $(TARGET).velf
	@cp $(TARGET).velf ~/shared/vitasample.elf
	@echo "Copied!"
