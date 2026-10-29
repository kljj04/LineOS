/* Link/Link.ld */
/* LineOS Project */
/* Copyright (C) 2026 LineOS Developer kljj04 */

ENTRY(KMain)

SECTIONS
{
    . = 0x4000000;

    KERNEL_START = .;

    .text ALIGN(4096) :
    {
        *(.text*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata*)
    }

    .data ALIGN(4096) :
    {
        *(.data*)
    }

    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss*)
    }

    KERNEL_END = .;
}