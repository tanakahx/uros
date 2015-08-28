ARCH := armv7-m
BOARD := lm3s6965evb
#BOARD := stm32f407

CC := arm-linux-gnueabi-gcc
LD := arm-linux-gnueabi-ld
OBJCOPY := arm-linux-gnueabi-objcopy

CFLAGS = -Wall -fno-builtin -Isrc -Iapp
LDFLAGS =
OBJS := src/kernel.o src/lib.o src/uart.o app/config.o app/main.o

CONFIGURATOR := util/config.lisp
CONFIG_INFO := app/config.json

TARGET := image

all:
	$(MAKE) app/config.c
	$(MAKE) $(TARGET)

include arch/$(ARCH)/Makefile

app/config.c: $(CONFIG_INFO)
	$(CONFIGURATOR) -d app $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET).elf: $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $^

$(TARGET): $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

.PHONY: clean

run:
	qemu-system-arm -M $(BOARD) -nographic -kernel $(TARGET)

serial:
	cu -s 115200 -l /dev/ttyUSB0

clean:
	-$(foreach obj, */*/*/*.o */*/*.o */*.o, rm $(obj);)
	-rm app/config.c app/config.h
	-rm $(TARGET) $(TARGET).elf
