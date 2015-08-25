MEMORY
{
        flash (rx)  : ORIGIN = 0x00000000, LENGTH = 0x40000
        sram (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
        stack (rwx) : ORIGIN = 0x20010000, LENGTH = 0K
}

SECTIONS {
         .vector : {
                 arch/armv7-m/lm3s6965evb/vector.o (.rodata*)
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
