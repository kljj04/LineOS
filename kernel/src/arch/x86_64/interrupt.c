// interrupt.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/interrupt.h>
#include <time/lapic.h>
#include <time/timer.h>

#define IDT_ENTRY_COUNT 256

STATIC IDT_ENTRY IDT[IDT_ENTRY_COUNT];
STATIC IDT_POINTER IDTPointer;

STATIC UINT16 IDTCodeSelector;

STATIC UINT16 ReadCS(VOID)
{
    UINT16 selector;

    __asm__ volatile("mov %%cs, %0" : "=r"(selector));
    return selector;
}

STATIC VOID Out8(UINT16 port, UINT8 value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

STATIC VOID MaskPIC(VOID)
{
    Out8(0x21, 0xFF);
    Out8(0xA1, 0xFF);
}

__attribute__((interrupt)) VOID DefaultInterrupt(INTERRUPT_FRAME* frame)
{
    (VOID)frame;

    LAPICEOI();
}

__attribute__((interrupt)) VOID LAPICTimerInterrupt(INTERRUPT_FRAME* frame)
{
    (VOID)frame;

    TimerHandleTick();
    LAPICEOI();
}

VOID IDTSetGate(UINT8 vector, VOID* handler, UINT8 TypeAttributes)
{
    UINT64 HandlerAddress = (UINT64)handler;

    IDT[vector].OffsetLow = (UINT16)HandlerAddress;
    IDT[vector].Selector = IDTCodeSelector;
    IDT[vector].IST = 0;
    IDT[vector].TypeAttributes = TypeAttributes;
    IDT[vector].OffsetMiddle = (UINT16)(HandlerAddress >> 16);
    IDT[vector].OffsetHigh = (UINT32)(HandlerAddress >> 32);
    IDT[vector].Reserved = 0;
}

VOID IDTInit(VOID)
{
    IDTCodeSelector = ReadCS();

    for (UINT32 i = 0; i < IDT_ENTRY_COUNT; i++)
    {
        IDTSetGate((UINT8)i, DefaultInterrupt, IDT_GATE_INTERRUPT);
    }

    IDTPointer.Limit = (UINT16)(sizeof(IDT) - 1);
    IDTPointer.Base = (UINT64)&IDT[0];

    IDTSetGate(INTERRUPT_LAPIC_TIMER_VECTOR, LAPICTimerInterrupt, IDT_GATE_INTERRUPT);
    MaskPIC();
    __asm__ volatile("lidt %0" : : "m"(IDTPointer));
}
