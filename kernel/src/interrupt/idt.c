// kernel/src/interrupt/idt.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <interrupt/idt.h>
#include <interrupt/apic.h>
#include <debug/panic.h>
#include <render/gpu/virtio_gpu.h>
#include <lineos/typeinfo.h>
#include <memory/memory.h>
#include <arch/x86_64/cpu.h>

EXTERN VOID ISR0(VOID);
EXTERN VOID ISR1(VOID);
EXTERN VOID ISR2(VOID);
EXTERN VOID ISR3(VOID);
EXTERN VOID ISR4(VOID);
EXTERN VOID ISR5(VOID);
EXTERN VOID ISR6(VOID);
EXTERN VOID ISR7(VOID);
EXTERN VOID ISR8(VOID);
EXTERN VOID ISR9(VOID);
EXTERN VOID ISR10(VOID);
EXTERN VOID ISR11(VOID);
EXTERN VOID ISR12(VOID);
EXTERN VOID ISR13(VOID);
EXTERN VOID ISR14(VOID);
EXTERN VOID ISR15(VOID);
EXTERN VOID ISR16(VOID);
EXTERN VOID ISR17(VOID);
EXTERN VOID ISR18(VOID);
EXTERN VOID ISR19(VOID);
EXTERN VOID ISR20(VOID);
EXTERN VOID ISR21(VOID);
EXTERN VOID ISR22(VOID);
EXTERN VOID ISR23(VOID);
EXTERN VOID ISR24(VOID);
EXTERN VOID ISR25(VOID);
EXTERN VOID ISR26(VOID);
EXTERN VOID ISR27(VOID);
EXTERN VOID ISR28(VOID);
EXTERN VOID ISR29(VOID);
EXTERN VOID ISR30(VOID);
EXTERN VOID ISR31(VOID);

EXTERN VOID ISR64(VOID);

STATIC VOID (*ISRTable[32])(VOID) = {ISR0, ISR1, ISR2, ISR3, ISR4, ISR5, ISR6, ISR7, ISR8, ISR9, ISR10, ISR11, ISR12, ISR13, ISR14, ISR15, ISR16, ISR17, ISR18, ISR19, ISR20, ISR21, ISR22, ISR23, ISR24, ISR25, ISR26, ISR27, ISR28, ISR29, ISR30, ISR31};

STATIC IDT_DESCRIPTOR IDTR;
STATIC IDT_ENTRY      IDT[256];

VOID IDTInit(VOID)
{
    UINT16 selector;

    KMemSet(IDT, 0, sizeof(IDT));

    IDTR.Limit = sizeof(IDT) - 1;
    IDTR.Base = (UINT64) IDT;

    ASM("mov %%cs, %0" : "=r"(selector));

    for (UINT8 vector = 0; vector < 32; vector++)
    {
        IDTSetGate(vector, (UINT64) ISRTable[vector], selector, 0, 0x8E);
    }
    IDTSetGate(64, (UINT64) ISR64, selector, 0, 0x8E);
}

VOID IDTSetGate(UINT8 vector, UINT64 handler, UINT16 selector, UINT8 IST, UINT8 TypeAttributes)
{
    IDT[vector].OffsetLow = handler & 0xFFFF;
    IDT[vector].Selector = selector;
    IDT[vector].IST = IST & 0x07;
    IDT[vector].TypeAttributes = TypeAttributes;
    IDT[vector].OffsetMiddle = (handler >> 16) & 0xFFFF;
    IDT[vector].OffsetHigh = (handler >> 32) & 0xFFFFFFFF;
    IDT[vector].Reserved = 0;
}

VOID IDTLoad(VOID)
{
    ASM("lidt %0" : : "m"(IDTR));
}

VOID SYSV_ABI IDTInterruptHandler(INTERRUPT_FRAME *frame)
{
    if (frame->Vector < 32)
    {
        ExceptionPanic(frame);
    }

    if (frame->Vector == 64)
    {
        LAPICEOI();
        return;
    }
}
