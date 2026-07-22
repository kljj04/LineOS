// cpu.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>
#include <arch/x86_64/cpu.h>

LINEOS_CPU_INFO CPUInfo;

VOID CPUID(UINT32 leaf, UINT32 subleaf, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx)
{
    UINT32 a;
    UINT32 b;
    UINT32 c;
    UINT32 d;

    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf));

    if (eax != NULL)
    {
        *eax = a;
    }

    if (ebx != NULL)
    {
        *ebx = b;
    }

    if (ecx != NULL)
    {
        *ecx = c;
    }

    if (edx != NULL)
    {
        *edx = d;
    }
}

VOID CPUDetect(VOID)
{
    UINT32 eax;
    UINT32 ebx;
    UINT32 ecx;
    UINT32 edx;

    CPUID(0x00000000, 0, &eax, &ebx, &ecx, &edx);
    CPUInfo.MaxLeaf = eax;

    CPUID(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    CPUInfo.MaxExtendedLeaf = eax;

    if (CPUInfo.MaxLeaf >= 0x00000001)
    {
        CPUID(0x00000001, 0, &eax, &ebx, &ecx, &edx);
        CPUInfo.TSC = (edx & (1U << 4)) != 0;
        CPUInfo.APIC = (edx & (1U << 9)) != 0;
        CPUInfo.X2APIC = (ecx & (1U << 21)) != 0;
        CPUInfo.TSCDeadline = (ecx & (1U << 24)) != 0;
    }

    if (CPUInfo.MaxExtendedLeaf >= 0x80000007)
    {
        CPUID(0x80000007, 0, &eax, &ebx, &ecx, &edx);
        CPUInfo.InvariantTSC = (edx & (1U << 8)) != 0;
    }
}

UINT64 RDMSR(UINT32 msr)
{
    UINT32 low;
    UINT32 high;

    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((UINT64)high << 32) | low;
}

VOID WRMSR(UINT32 msr, UINT64 value)
{
    UINT32 low = (UINT32)value;
    UINT32 high = (UINT32)(value >> 32);

    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

VOID HLT(VOID)
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
