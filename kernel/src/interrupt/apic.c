// kernel/src/interrupt/apic.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <interrupt/apic.h>
#include <arch/x86_64/cpu.h>
#include <lineos/typeinfo.h>
#include <lineos/bootinfo.h>

#define IA32_APIC_BASE_MSR     0x1B
#define IA32_APIC_BASE_ENABLE  (1ULL << 11)
#define IA32_APIC_BASE_ADDRESS 0xFFFFF000ULL

#define LAPIC_REG_ID  0x020
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_SVR 0x0F0

#define LAPIC_SVR_ENABLE (1U << 8)

#define LAPIC_REG_LVT_TIMER      0x320
#define LAPIC_TIMER_TSC_DEADLINE (2U << 17)

STATIC VOLATILE UINT32 *LAPICBase = NULL;

STATIC BOOLEAN LAPICSupported(VOID)
{
    UINT32 eax;
    UINT32 ebx;
    UINT32 ecx;
    UINT32 edx;

    eax = 1;

    ASM("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));

    return (edx & (1U << 9)) != 0;
}

BOOLEAN LAPICInit(VOID)
{
    UINT64 APICBaseMSR;
    UINT32 SVR;

    if (!LAPICSupported())
    {
        return FALSE;
    }

    APICBaseMSR = ReadMSR(IA32_APIC_BASE_MSR);

    if ((APICBaseMSR & IA32_APIC_BASE_ENABLE) == 0)
    {
        APICBaseMSR |= IA32_APIC_BASE_ENABLE;
        WriteMSR(IA32_APIC_BASE_MSR, APICBaseMSR);
    }

    LAPICBase = (VOLATILE UINT32 *) (APICBaseMSR & IA32_APIC_BASE_ADDRESS);

    SVR = LAPICRead(LAPIC_REG_SVR);
    SVR &= ~0xFFU;
    SVR |= LAPIC_SPURIOUS_VECTOR;
    SVR |= LAPIC_SVR_ENABLE;
    LAPICWrite(LAPIC_REG_SVR, SVR);

    return TRUE;
}

UINT32 LAPICRead(UINT32 reg)
{
    return LAPICBase[reg / sizeof(UINT32)];
}

VOID LAPICWrite(UINT32 reg, UINT32 value)
{
    LAPICBase[reg / sizeof(UINT32)] = value;
}

VOID LAPICEOI(VOID)
{
    LAPICWrite(LAPIC_REG_EOI, 0);
}

UINT32 LAPICGetID(VOID)
{
    return LAPICRead(LAPIC_REG_ID) >> 24;
}

VOID LAPICSetTSCDeadlineTimer(UINT8 vector)
{
    LAPICWrite(LAPIC_REG_LVT_TIMER, (UINT32) vector | LAPIC_TIMER_TSC_DEADLINE);
}