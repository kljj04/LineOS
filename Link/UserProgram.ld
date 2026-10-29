/* Link/UserProgram.ld */
/* LineOS Project */
/* Copyright (C) 2026 LineOS Developer kljj04 */

ENTRY(_Start)

PHDRS
{
    Text PT_LOAD FILEHDR PHDRS FLAGS(5);
    Data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x400000000000 + SIZEOF_HEADERS;

    .text :
    {
        *(.text .text.* .ltext .ltext.*)
    } :Text

    .rodata :
    {
        *(.rodata .rodata.* .lrodata .lrodata.*)
    } :Text

    . = ALIGN(4096);

    .data :
    {
        *(.data .data.* .ldata .ldata.*)
    } :Data

    .bss :
    {
        *(COMMON)
        *(.bss .bss.* .lbss .lbss.*)
    } :Data

    /DISCARD/ :
    {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
    }
}
