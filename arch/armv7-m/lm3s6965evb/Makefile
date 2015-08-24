CFLAGS  += -Iarch/$(ARCH)/$(BOARD) -D LM3S6965EVB
LDFLAGS += -T arch/$(ARCH)/$(BOARD)/arm.ld
OBJS := arch/$(ARCH)/$(BOARD)/vector.o \
	arch/$(ARCH)/$(BOARD)/init.o \
	arch/$(ARCH)/$(BOARD)/uart_hal.o \
	$(OBJS)
