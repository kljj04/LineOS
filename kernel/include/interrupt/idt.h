// kernel/include/interrupt/idt.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

typedef struct PACKED
{
    UINT16 OffsetLow;
    UINT16 Selector;
    UINT8  IST;
    UINT8  TypeAttributes;
    UINT16 OffsetMiddle;
    UINT32 OffsetHigh;
    UINT32 Reserved;
} IDT_ENTRY;

typedef struct PACKED
{
    UINT16 Limit;
    UINT64 Base;
} IDT_DESCRIPTOR;

typedef struct
{
    UINT64 R15;
    UINT64 R14;
    UINT64 R13;
    UINT64 R12;
    UINT64 R11;
    UINT64 R10;
    UINT64 R9;
    UINT64 R8;
    UINT64 RBP;
    UINT64 RDI;
    UINT64 RSI;
    UINT64 RDX;
    UINT64 RCX;
    UINT64 RBX;
    UINT64 RAX;

    UINT64 Vector;
    UINT64 ErrorCode;

    UINT64 RIP;
    UINT64 CS;
    UINT64 RFLAGS;
    UINT64 RSP;
    UINT64 SS;
} INTERRUPT_FRAME;

VOID            IDTInit(VOID);
VOID            IDTSetGate(UINT8 vector, UINT64 handler, UINT16 selector, UINT8 IST, UINT8 TypeAttributes);
VOID            IDTLoad(VOID);
UINT64 SYSV_ABI IDTInterruptHandler(INTERRUPT_FRAME *frame);
