CFLAGS  += -Iarch/$(ARCH)/$(BOARD) -Iarch/$(ARCH)/$(BOARD)/STM32CubeF4 -D STM32F407xx
LDFLAGS += -T arch/$(ARCH)/$(BOARD)/arm.ld
OBJS := arch/$(ARCH)/$(BOARD)/vector.o \
	arch/$(ARCH)/$(BOARD)/init.o \
	arch/$(ARCH)/$(BOARD)/uart_hal.o \
	arch/$(ARCH)/$(BOARD)/STM32CubeF4/system_stm32f4xx.o \
	$(OBJS)

FLASH_WRITER := openocd
#FLASH_WRITER := st-flash

flash:
ifeq ($(FLASH_WRITER),openocd)
	openocd -s /usr/local/share/openocd/scripts/ -f board/stm32f4discovery.cfg -c "program $(TARGET) verify reset exit 0x08000000"
else
	st-flash --reset write $(TARGET) 0x08000000
endif
