// Kernel/Include/Interrupt/APIC.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define LAPIC_SPURIOUS_VECTOR 0xFF

BOOLEAN LAPICInit(VOID);
BOOLEAN LAPICInitCurrentCPU(VOID);
UINT32  LAPICRead(UINT32 Reg);
VOID    LAPICWrite(UINT32 Reg, UINT32 Value);
VOID    LAPICEOI(VOID);
UINT32  LAPICGetID(VOID);
VOID    LAPICSetTSCDeadlineTimer(UINT8 Vector);
BOOLEAN LAPICSendINIT(UINT32 APICID);
BOOLEAN LAPICSendSIPI(UINT32 APICID, UINT8 Vector);
