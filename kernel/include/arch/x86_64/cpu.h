// cpu.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

VOID   HLT();
VOID   CLI();
VOID   STI();
VOID   PAUSE();
UINT8  INB(UINT16 Port);
UINT16 INW(UINT16 Port);
UINT32 INL(UINT16 Port);
VOID   OUTB(UINT16 Port, UINT8 Value);
VOID   OUTW(UINT16 Port, UINT16 Value);
VOID   OUTL(UINT16 Port, UINT32 Value);
VOID   CPUID(UINT32 leaf, UINT32 subleaf, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx);