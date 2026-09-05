// kernel/include/interrupt/apic.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

#define LAPIC_SPURIOUS_VECTOR 0xFF

BOOLEAN LAPICInit(VOID);
UINT32  LAPICRead(UINT32 reg);
VOID    LAPICWrite(UINT32 reg, UINT32 value);
VOID    LAPICEOI(VOID);
UINT32  LAPICGetID(VOID);
VOID    LAPICSetTSCDeadlineTimer(UINT8 vector);
BOOLEAN LAPICSendINIT(UINT32 APICID);
BOOLEAN LAPICSendSIPI(UINT32 APICID, UINT8 Vector);
