// cpu.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>

VOID Halt(VOID)
{
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

VOID CLI(VOID)
{
    __asm__ volatile("cli");
}


VOID STI(VOID)
{
    __asm__ volatile("sti");
}