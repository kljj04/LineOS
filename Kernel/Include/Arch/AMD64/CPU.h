// Kernel/Include/Arch/AMD64/CPU.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_KERNEL_CODE_SELECTOR 0x18
#define GDT_MAX_CPUS             256
#define GDT_ENTRY_COUNT          8
#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_KERNEL_CODE_SELECTOR 0x18
#define GDT_USER_CODE_SELECTOR   0x20
#define GDT_USER_DATA_SELECTOR   0x28
#define GDT_TSS_SELECTOR         0x30

#define GDT_IST_STACK_SIZE (16 * 1024)

typedef struct PACKED
{
    UINT16 Limit;
    UINT64 Base;
} GDT_DESCRIPTOR;

typedef struct PACKED
{
    UINT32 Reserved0;
    UINT64 RSP0;
    UINT64 RSP1;
    UINT64 RSP2;
    UINT64 Reserved1;
    UINT64 IST1;
    UINT64 IST2;
    UINT64 IST3;
    UINT64 IST4;
    UINT64 IST5;
    UINT64 IST6;
    UINT64 IST7;
    UINT64 Reserved2;
    UINT16 Reserved3;
    UINT16 IOPBOffset;
} TSS_64;

VOID    HLT();
VOID    HLTONCE();
VOID    STIHLT();
VOID    CLI();
VOID    STI();
VOID    PAUSE();
VOID    CompilerBarrier();
VOID    GDTInitCurrentCPU(UINT32 CPUID);
BOOLEAN GDTSetKernelStack(UINT64 StackTop);
VOID    CPUEnableSSE(VOID);
UINT64  RDTSC();
UINT8   INB(UINT16 Port);
UINT16  INW(UINT16 Port);
UINT32  INL(UINT16 Port);
VOID    OUTB(UINT16 Port, UINT8 Value);
VOID    OUTW(UINT16 Port, UINT16 Value);
VOID    OUTL(UINT16 Port, UINT32 Value);
VOID    CPUID(UINT32 Leaf, UINT32 Subleaf, UINT32 *EAX, UINT32 *EBX, UINT32 *ECX, UINT32 *EDX);
UINT64  ReadMSR(UINT32 MSR);
VOID    WriteMSR(UINT32 MSR, UINT64 Value);
