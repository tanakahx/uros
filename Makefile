CPU := cortex-m3

ifeq ($(CPU), cortex-m3)
MACHINE := lm3s6965evb
VMA := 0x00000000
endif

CC := arm-none-eabi-gcc
CFLAGS = -Wall -fno-builtin -Isrc

LD := ld-arm
LDFLAGS =

OBJS := src/start.o src/main.o src/lib.o src/uart.o

include cpu/$(CPU)/Makefile

TARGET := image

$(TARGET): $(TARGET).elf
	objcopy-arm -O binary $< $@

$(TARGET).elf: $(OBJS)
	$(LD) -o $@ $(LDFLAGS) $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: dis run clean

run:
	qemu-system-arm -M $(MACHINE) -nographic -kernel $(TARGET)

clean:
	-@$(foreach obj, */*/*.o */*.o, rm $(obj);)
	-rm $(TARGET) $(TARGET).elf
