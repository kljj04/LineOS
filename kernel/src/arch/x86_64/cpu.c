// kernel/src/arch/x86_64/cpu.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <lineos/typeinfo.h>

EXTERN VOID LBPWRRRecordIdleTSC(UINT64 StartTSC, UINT64 EndTSC);

#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_KERNEL_CODE_SELECTOR 0x18

typedef struct PACKED
{
    UINT16 Limit;
    UINT64 Base;
} GDT_DESCRIPTOR;

STATIC UINT64 GDT[] = {
    0x0000000000000000ULL,
    0x00CF9A000000FFFFULL,
    0x00CF92000000FFFFULL,
    0x00AF9A000000FFFFULL,
};

STATIC GDT_DESCRIPTOR GDTR = {
    sizeof(GDT) - 1,
    (UINT64) GDT,
};

VOID HLT()
{
    while (TRUE)
    {
        ASM("hlt");
    }
}

VOID HLTONCE()
{
    UINT64 StartTSC;
    UINT64 EndTSC;

    StartTSC = RDTSC();
    ASM("hlt");
    EndTSC = RDTSC();

    LBPWRRRecordIdleTSC(StartTSC, EndTSC);
}

VOID STIHLT()
{
    ASM("sti; hlt" ::: "memory");
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

VOID CompilerBarrier()
{
    ASM("" ::: "memory");
}

VOID GDTInitCurrentCPU(VOID)
{
    ASM(
        "lgdt %0\n"
        "movw %1, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "pushq %2\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        : "m"(GDTR), "i"(GDT_KERNEL_DATA_SELECTOR), "i"(GDT_KERNEL_CODE_SELECTOR)
        : "rax", "memory");
}

UINT64 RDTSC()
{
    UINT32 low;
    UINT32 high;

    ASM("rdtsc" : "=a"(low), "=d"(high));

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
