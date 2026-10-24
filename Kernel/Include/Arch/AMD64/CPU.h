// Kernel/Include/Arch/AMD64/CPU.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

VOID   HLT();
VOID   HLTONCE();
VOID   STIHLT();
VOID   CLI();
VOID   STI();
VOID   PAUSE();
VOID   CompilerBarrier();
VOID   GDTInitCurrentCPU(VOID);
BOOLEAN GDTSetKernelStack(UINT64 StackTop);
VOID   CPUEnableSSE(VOID);
UINT64 RDTSC();
UINT8  INB(UINT16 Port);
UINT16 INW(UINT16 Port);
UINT32 INL(UINT16 Port);
VOID   OUTB(UINT16 Port, UINT8 Value);
VOID   OUTW(UINT16 Port, UINT16 Value);
VOID   OUTL(UINT16 Port, UINT32 Value);
VOID   CPUID(UINT32 Leaf, UINT32 Subleaf, UINT32 *EAX, UINT32 *EBX, UINT32 *ECX, UINT32 *EDX);
UINT64 ReadMSR(UINT32 MSR);
VOID   WriteMSR(UINT32 MSR, UINT64 Value);
