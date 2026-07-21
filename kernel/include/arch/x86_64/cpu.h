// cpu.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef struct
{
    UINT32 MaxLeaf;
    UINT32 MaxExtendedLeaf;
    BOOLEAN TSC;
    BOOLEAN InvariantTSC;
    BOOLEAN APIC;
    BOOLEAN X2APIC;
    BOOLEAN TSCDeadline;
} LINEOS_CPU_INFO;

VOID CPUID(UINT32 leaf, UINT32 subleaf, UINT32* eax, UINT32* ebx, UINT32* ecx, UINT32* edx);
VOID CPUDetect(VOID);
VOID HLT(VOID);
VOID CLI(VOID);
VOID STI(VOID);

extern LINEOS_CPU_INFO CPUInfo;
