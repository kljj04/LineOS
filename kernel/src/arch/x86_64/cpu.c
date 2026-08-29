// cpu.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <lineos/typeinfo.h>

VOID HLT()
{
    while (TRUE)
    {
        ASM("hlt");
    }
}

VOID CLI()
{
    ASM("cli" ::: "memory");
}

VOID STI()
{
    ASM("sti" ::: "memory");
}

VOID PAUSE()
{
    ASM("pause");
}

UINT8 INB(UINT16 Port)
{
    UINT8 Value;

    ASM("inb %w1, %b0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

UINT16 INW(UINT16 Port)
{
    UINT16 Value;

    ASM("inw %w1, %w0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

UINT32 INL(UINT16 Port)
{
    UINT32 Value;

    ASM("inl %w1, %0" : "=a"(Value) : "Nd"(Port));
    return Value;
}

VOID OUTB(UINT16 Port, UINT8 Value)
{
    ASM("outb %b0, %w1" : : "a"(Value), "Nd"(Port));
}

VOID OUTW(UINT16 Port, UINT16 Value)
{
    ASM("outw %w0, %w1" : : "a"(Value), "Nd"(Port));
}

VOID OUTL(UINT16 Port, UINT32 Value)
{
    ASM("outl %0, %w1" : : "a"(Value), "Nd"(Port));
}

VOID CPUID(UINT32 leaf, UINT32 subleaf, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx)
{
    ASM("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(subleaf));
}