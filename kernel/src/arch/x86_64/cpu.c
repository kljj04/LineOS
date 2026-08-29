// kernel/src/arch/x86_64/cpu.c
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

VOID HLTONCE()
{
    ASM("hlt");
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

UINT64 RDTSC()
{
    UINT32 low;
    UINT32 high;

    ASM ("rdtsc" : "=a"(low), "=d"(high));

    return ((UINT64) high << 32) | low;
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

UINT64 ReadMSR(UINT32 msr)
{
    UINT32 low;
    UINT32 high;

    ASM("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

    return ((UINT64) high << 32) | low;
}

VOID WriteMSR(UINT32 msr, UINT64 value)
{
    UINT32 low = (UINT32) value;
    UINT32 high = (UINT32) (value >> 32);

    ASM("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}
