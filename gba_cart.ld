/* Linker Script for GBA ROMs */
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    ewram   : ORIGIN = 0x02000000, LENGTH = 256K
    iwram   : ORIGIN = 0x03000000, LENGTH = 32K
    rom     : ORIGIN = 0x08000000, LENGTH = 32M
}

SECTIONS {
    .text : {
        *(.text.start)
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } >rom = 0xff

    .data : {
        *(.data*)
        . = ALIGN(4);
    } >ewram AT>rom

    .bss : {
        *(.bss*)
        . = ALIGN(4);
    } >ewram

    /* Discard unused sections */
    /DISCARD/ : {
        *(.ARM.*)
        *(.comment)
        *(.gnu.*)
    }

    /* Set heap end and stack */
    __end__ = .;
    __stack_top__ = 0x03000000 + 32K;
} 