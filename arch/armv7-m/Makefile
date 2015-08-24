CFLAGS  += -march=armv7-m -mthumb -Iarch/$(ARCH)/

OBJS := arch/$(ARCH)/system.o \
	arch/$(ARCH)/default_handler.o \
	$(OBJS)

include arch/$(ARCH)/$(BOARD)/Makefile
