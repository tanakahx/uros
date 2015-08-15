MEMORY
{
        vector (r)  : ORIGIN = 0x00000000, LENGTH = 0x400
        flash (rx)  : ORIGIN = 0x00000404, LENGTH = 0x3FF00
        sram (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
        load (rwx)  : ORIGIN = 0x20000100, LENGTH = 0xFF00
        stack (rwx) : ORIGIN = 0x20010000, LENGTH = 0K
}

SECTIONS {
         .vector : {
                 cpu/cortex-m3/vector.o (.rodata*)
         } > vector

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
         } > load AT > flash
}
