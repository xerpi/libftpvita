#
# Copyright (c) 2015 Sergi Granell (xerpi)
# based on Cirne's vita-toolchain test Makefile
#

TARGET = FTPVita
OBJS   = main.o ftp.o console.o draw.o font_data.o

LIBS = -lc -lSceKernel_stub -lSceDisplay_stub -lSceGxm_stub	\
	-lSceNet_stub -lSceNetCtl_stub -lSceCtrl_stub

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
READELF = $(PREFIX)-readelf
OBJDUMP = $(PREFIX)-objdump
CFLAGS  = -Wl,-q -Wall -O3
ASFLAGS = $(CFLAGS)

all: $(TARGET).velf

debug: CFLAGS += -DSHOW_DEBUG=1
debug: all

%.velf: %.elf
	$(PREFIX)-strip -g $<
	vita-elf-create $< $@ > /dev/null

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	@rm -rf $(TARGET).velf $(TARGET).elf $(OBJS)

copy: $(TARGET).velf
	@cp $(TARGET).velf ~/shared/vitasample.elf
	@echo "Copied!"

send: $(TARGET).velf
	curl -T $(TARGET).velf ftp://$(PSVITAIP):1337/cache0:/
	@echo "Sent."

run: $(TARGET).velf
	@sh run_homebrew_unity.sh $(TARGET).velf
