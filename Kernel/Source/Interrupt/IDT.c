// kernel/src/interrupt/IDT.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Interrupt/IDT.h>

#include <Debug/Debug.h>
#include <Disk/VirtIOBlock.h>
#include <Timer/TSC.h>

#include <Scheduler/LBPWRR.h>
#include <Interrupt/APIC.h>
#include <Debug/Panic.h>
#include <Input/VirtIOInput.h>
#include <Render/GPU/VirtIOGPU.h>
#include <LineOS/TypeInfo.h>
#include <Memory/PMM.h>
#include <Arch/AMD64/CPU.h>

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
EXTERN VOID ISR65(VOID);
EXTERN VOID ISR66(VOID);
EXTERN VOID ISR67(VOID);
EXTERN VOID ISR68(VOID);

STATIC VOID (*ISRTable[32])(VOID) = {ISR0, ISR1, ISR2, ISR3, ISR4, ISR5, ISR6, ISR7, ISR8, ISR9, ISR10, ISR11, ISR12, ISR13, ISR14, ISR15, ISR16, ISR17, ISR18, ISR19, ISR20, ISR21, ISR22, ISR23, ISR24, ISR25, ISR26, ISR27, ISR28, ISR29, ISR30, ISR31};

STATIC IDT_DESCRIPTOR IDTR;
STATIC IDT_ENTRY      IDT[256];

VOID IDTInit(VOID)
{
    UINT16 Selector;

    KMemSet(IDT, 0, sizeof(IDT));

    IDTR.Limit = sizeof(IDT) - 1;
    IDTR.Base = (UINT64) IDT;

    ASM("mov %%cs, %0" : "=r"(Selector));

    for (UINT8 Vector = 0; Vector < 32; Vector++)
    {
        IDTSetGate(Vector, (UINT64) ISRTable[Vector], Selector, 0, 0x8E);
    }
    IDTSetGate(64, (UINT64) ISR64, Selector, 0, 0x8E);
    IDTSetGate(65, (UINT64) ISR65, Selector, 0, 0x8E);
    IDTSetGate(66, (UINT64) ISR66, Selector, 0, 0x8E);
    IDTSetGate(67, (UINT64) ISR67, Selector, 0, 0x8E);
    IDTSetGate(68, (UINT64) ISR68, Selector, 0, 0x8E);
}

VOID IDTSetGate(UINT8 Vector, UINT64 Handler, UINT16 Selector, UINT8 IST, UINT8 TypeAttributes)
{
    IDT[Vector].OffsetLow = Handler & 0xFFFF;
    IDT[Vector].Selector = Selector;
    IDT[Vector].IST = IST & 0x07;
    IDT[Vector].TypeAttributes = TypeAttributes;
    IDT[Vector].OffsetMiddle = (Handler >> 16) & 0xFFFF;
    IDT[Vector].OffsetHigh = (Handler >> 32) & 0xFFFFFFFF;
    IDT[Vector].Reserved = 0;
}

VOID IDTLoad(VOID)
{
    ASM("lidt %0" : : "m"(IDTR));
}

UINT64 SYSV_ABI IDTInterruptHandler(INTERRUPT_FRAME *Frame)
{
    if (Frame->Vector < 32)
    {
        ExceptionPanic(Frame);
    }

    if (Frame->Vector == 64)
    {
        UINT64 NextRSP;

        TSCSetDeadline(RDTSC() + TSCGetFrequency() / 200);

        NextRSP = LBPWRRTick(Frame);

        LAPICEOI();

        return NextRSP;
    }

    if (Frame->Vector == 65)
    {
        VirtIOKeyboardInterruptHandler();

        LAPICEOI();

        return (UINT64) Frame;
    }

    if (Frame->Vector == 66)
    {
        VirtIOTabletInterruptHandler();

        LAPICEOI();

        return (UINT64) Frame;
    }

    if (Frame->Vector == 67)
    {
        UINT64 RSP;

        RSP = LBPWRRTick(Frame);

        LAPICEOI();

        return RSP;
    }

    if (Frame->Vector == 68)
    {
        VirtIOBlockHandleInterrupt(VirtIOBlockGetDevice());

        LAPICEOI();

        return (UINT64) Frame;
    }

    return (UINT64) Frame;
}
