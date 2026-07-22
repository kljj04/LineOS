// interrupt.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

#define IDT_GATE_INTERRUPT 0x8E
#define IDT_KERNEL_CODE_SELECTOR 0x08
#define INTERRUPT_LAPIC_TIMER_VECTOR 0x40

typedef struct
{
    UINT16 OffsetLow;
    UINT16 Selector;
    UINT8 IST;
    UINT8 TypeAttributes;
    UINT16 OffsetMiddle;
    UINT32 OffsetHigh;
    UINT32 Reserved;
} PACKED IDT_ENTRY;

typedef struct
{
    UINT16 Limit;
    UINT64 Base;
} PACKED IDT_POINTER;

typedef struct
{
    UINT64 RIP;
    UINT64 CS;
    UINT64 RFlags;
    UINT64 RSP;
    UINT64 SS;
} INTERRUPT_FRAME;

VOID IDTInit(VOID);
VOID IDTSetGate(UINT8 vector, VOID* handler, UINT8 TypeAttributes);

