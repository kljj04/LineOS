// cpu.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <lineos/bootinfo.h>

VOID HLT()
{
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

VOID CLI()
{
    __asm__ volatile("cli" ::: "memory");
}

VOID STI()
{
    __asm__ volatile("sti" ::: "memory");
}

VOID PAUSE()
{
    __asm__ volatile("pause");
}

UINT8 INB(UINT16 Port)
{
    UINT8 Value;

    __asm__ volatile("inb %w1, %b0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

UINT16 INW(UINT16 Port)
{
    UINT16 Value;

    __asm__ volatile("inw %w1, %w0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

UINT32 INL(UINT16 Port)
{
    UINT32 Value;

    __asm__ volatile("inl %w1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

VOID OUTB(UINT16 Port, UINT8 Value)
{
    __asm__ volatile("outb %b0, %w1" : : "a"(Value), "Nd"(Port));
}

VOID OUTW(UINT16 Port, UINT16 Value)
{
    __asm__ volatile("outw %w0, %w1" : : "a"(Value), "Nd"(Port));
}

VOID OUTL(UINT16 Port, UINT32 Value)
{
    __asm__ volatile("outl %0, %w1" : : "a"(Value), "Nd"(Port));
}
