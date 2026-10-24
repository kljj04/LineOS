/* Link/UserProgram.ld */
/* LineOS Project */
/* Copyright (C) 2026 LineOS Developer kljj04 */

ENTRY(_start)

PHDRS
{
    Text PT_LOAD FILEHDR PHDRS FLAGS(5);
    Data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x400000 + SIZEOF_HEADERS;

    .text :
    {
        *(.text .text.*)
    } :Text

    .rodata :
    {
        *(.rodata .rodata.*)
    } :Text

    . = ALIGN(4096);

    .data :
    {
        *(.data .data.*)
    } :Data

    .bss :
    {
        *(COMMON)
        *(.bss .bss.*)
    } :Data

    /DISCARD/ :
    {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
    }
}
