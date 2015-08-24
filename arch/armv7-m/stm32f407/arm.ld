MEMORY
{
        flash (rx)  : ORIGIN = 0x00000000, LENGTH = 1M
        sram (rwx)  : ORIGIN = 0x20000000, LENGTH = 128K
        stack (rwx) : ORIGIN = 0x20020000, LENGTH = 0
}

SECTIONS {
         .vector : {
                 arch/armv7-m/stm32f407/vector.o (.rodata*)
         } > flash

         .text : {
               * (.text)
         } > flash

         .rodata : {
                 rodata_start = .;
                 * (.rodata*)
                 rodata_end = .;
         } > flash
         
         .data : {
               data_start = .;
               * (.data)
               data_end = .;
         } > sram AT > flash
}

stack_bottom = ORIGIN(stack);
sram_start   = ORIGIN(sram);
sram_end     = ORIGIN(sram) + LENGTH(sram);
