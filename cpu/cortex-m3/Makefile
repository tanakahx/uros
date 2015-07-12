CFLAGS += -march=armv7-m -mcpu=$(CPU) -mthumb -Icpu/$(CPU)
LDFLAGS += -T cpu/$(CPU)/arm.ld
OBJS := cpu/$(CPU)/vector.o $(OBJS)
