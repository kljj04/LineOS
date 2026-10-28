// Kernel/Source/Arch/AMD64/CPU.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Arch/AMD64/CPU.h>
#include <Syscall/Syscall.h>

#include "Debug/Debug.h"

#include <Multicore/SMP.h>
#include <Memory/PMM.h>
#include <LineOS/TypeInfo.h>

STATIC UINT64         GDT[GDT_MAX_CPUS][GDT_ENTRY_COUNT];
STATIC GDT_DESCRIPTOR GDTR[GDT_MAX_CPUS];
STATIC TSS_64         TSS[GDT_MAX_CPUS];
STATIC BOOLEAN        GDTInitialized[GDT_MAX_CPUS];
STATIC UINT8          GDTNMIStacks[GDT_MAX_CPUS][GDT_IST_STACK_SIZE] __attribute__((aligned(16)));
STATIC UINT8          GDTISTStacks[GDT_MAX_CPUS][GDT_IST_STACK_SIZE] __attribute__((aligned(16)));

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

VOID CPUEnableSSE(VOID)
{
    UINT64 CR0;
    UINT64 CR4;

    ASM("mov %%cr0, %0" : "=r"(CR0));
    CR0 &= ~(1ULL << 2);
    CR0 |= (1ULL << 1);
    CR0 |= (1ULL << 5);
    ASM("mov %0, %%cr0" : : "r"(CR0) : "memory");

    ASM("mov %%cr4, %0" : "=r"(CR4));
    CR4 |= (1ULL << 9);
    CR4 |= (1ULL << 10);
    ASM("mov %0, %%cr4" : : "r"(CR4) : "memory");

    ASM("fninit" ::: "memory");
}

STATIC BOOLEAN GDTGetCurrentCPUIndex(UINT32 *CPUIndex)
{
    if (CPUIndex == NULL)
    {
        return FALSE;
    }

    if (!SMPIsReady())
    {
        *CPUIndex = 0;
        return TRUE;
    }

    *CPUIndex = SMPGetCurrentCPUID();

    return *CPUIndex < GDT_MAX_CPUS;
}

VOID GDTInitCurrentCPU(UINT32 CPUID)
{
    UINT32 CPUIndex;
    UINT64 TSSBase;
    UINT64 TSSLimit;

    CPUIndex = CPUID;

    if (CPUIndex >= GDT_MAX_CPUS)
    {
        HLT();
    }

    KMemSet(GDT[CPUIndex], 0, sizeof(GDT[CPUIndex]));
    KMemSet(&TSS[CPUIndex], 0, sizeof(TSS[CPUIndex]));

    GDT[CPUIndex][0] = 0;
    GDT[CPUIndex][1] = 0x00AF9A000000FFFFULL;
    GDT[CPUIndex][2] = 0x00CF92000000FFFFULL;
    GDT[CPUIndex][3] = 0x00CFF2000000FFFFULL;
    GDT[CPUIndex][4] = 0x00AFFA000000FFFFULL;
    GDT[CPUIndex][5] = 0;

    TSS[CPUIndex].IST1 = (UINT64) &GDTISTStacks[CPUIndex][GDT_IST_STACK_SIZE];
    TSS[CPUIndex].IST2 = (UINT64) &GDTNMIStacks[CPUIndex][GDT_IST_STACK_SIZE];
    TSS[CPUIndex].IOPBOffset = sizeof(TSS_64);

    TSSBase = (UINT64) &TSS[CPUIndex];
    TSSLimit = sizeof(TSS_64) - 1;

    GDT[CPUIndex][6] = (TSSLimit & 0xFFFFULL) | ((TSSBase & 0xFFFFFFULL) << 16) | (0x89ULL << 40) | ((TSSLimit & 0xF0000ULL) << 32) | ((TSSBase & 0xFF000000ULL) << 32);
    GDT[CPUIndex][7] = TSSBase >> 32;

    GDTR[CPUIndex].Limit = sizeof(GDT[CPUIndex]) - 1;
    GDTR[CPUIndex].Base = (UINT64) GDT[CPUIndex];

    GDTInitialized[CPUIndex] = TRUE;

    ASM("lgdt %0\n"
        "movw %1, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "pushq %2\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x30, %%ax\n"
        "ltr %%ax\n"
        :
        : "m"(GDTR[CPUIndex]), "i"(GDT_KERNEL_DATA_SELECTOR), "i"(GDT_KERNEL_CODE_SELECTOR)
        : "rax", "memory");

    SyscallInitCurrentCPU(CPUIndex);
}

BOOLEAN GDTSetKernelStack(UINT64 StackTop)
{
    UINT32 CPUIndex;

    if (StackTop == 0 || (StackTop & 0xFULL) != 0)
    {
        return FALSE;
    }

    if (!GDTGetCurrentCPUIndex(&CPUIndex))
    {
        return FALSE;
    }

    if (!GDTInitialized[CPUIndex])
    {
        return FALSE;
    }

    TSS[CPUIndex].RSP0 = StackTop;
    SyscallSetKernelStack(CPUIndex, StackTop);

    CompilerBarrier();

    return TRUE;
}

UINT64 RDTSC()
{
    UINT32 Low;
    UINT32 High;

    ASM("rdtsc" : "=a"(Low), "=d"(High));

    return ((UINT64) High << 32) | Low;
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

VOID CPUID(UINT32 Leaf, UINT32 Subleaf, UINT32 *EAX, UINT32 *EBX, UINT32 *ECX, UINT32 *EDX)
{
    ASM("cpuid" : "=a"(*EAX), "=b"(*EBX), "=c"(*ECX), "=d"(*EDX) : "a"(Leaf), "c"(Subleaf));
}

UINT64 ReadMSR(UINT32 MSR)
{
    UINT32 Low;
    UINT32 High;

    ASM("rdmsr" : "=a"(Low), "=d"(High) : "c"(MSR));

    return ((UINT64) High << 32) | Low;
}

VOID WriteMSR(UINT32 MSR, UINT64 Value)
{
    UINT32 Low = (UINT32) Value;
    UINT32 High = (UINT32) (Value >> 32);

    ASM("wrmsr" : : "c"(MSR), "a"(Low), "d"(High));
}
