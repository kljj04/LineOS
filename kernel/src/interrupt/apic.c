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

#define LAPIC_REG_ICR_LOW  0x300
#define LAPIC_REG_ICR_HIGH 0x310

#define LAPIC_SVR_ENABLE (1U << 8)

#define LAPIC_REG_LVT_TIMER      0x320
#define LAPIC_TIMER_TSC_DEADLINE (2U << 17)

#define LAPIC_ICR_DELIVERY_STATUS (1U << 12)
#define LAPIC_ICR_LEVEL_ASSERT    (1U << 14)
#define LAPIC_ICR_TRIGGER_LEVEL   (1U << 15)

#define LAPIC_ICR_DELIVERY_INIT    (5U << 8)
#define LAPIC_ICR_DELIVERY_STARTUP (6U << 8)

#define LAPIC_IPI_WAIT_RETRIES 1000000

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
    return LAPICBase[LAPIC_REG_ID / sizeof(UINT32)] >> 24;
}

VOID LAPICSetTSCDeadlineTimer(UINT8 vector)
{
    LAPICWrite(LAPIC_REG_LVT_TIMER, (UINT32) vector | LAPIC_TIMER_TSC_DEADLINE);
}

STATIC BOOLEAN LAPICWaitForIPI(VOID)
{
    for (UINT32 retry = 0; retry < LAPIC_IPI_WAIT_RETRIES; retry++)
    {
        if ((LAPICRead(LAPIC_REG_ICR_LOW) & LAPIC_ICR_DELIVERY_STATUS) == 0)
        {
            return TRUE;
        }

        ASM("pause" ::: "memory");
    }

    return FALSE;
}

BOOLEAN LAPICSendINIT(UINT32 APICID)
{
    if (!LAPICWaitForIPI())
    {
        return FALSE;
    }

    LAPICWrite(LAPIC_REG_ICR_HIGH, APICID << 24);
    LAPICWrite(LAPIC_REG_ICR_LOW, LAPIC_ICR_DELIVERY_INIT | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_TRIGGER_LEVEL);

    if (!LAPICWaitForIPI())
    {
        return FALSE;
    }

    LAPICWrite(LAPIC_REG_ICR_HIGH, APICID << 24);
    LAPICWrite(LAPIC_REG_ICR_LOW, LAPIC_ICR_DELIVERY_INIT | LAPIC_ICR_TRIGGER_LEVEL);

    return LAPICWaitForIPI();
}

BOOLEAN LAPICSendSIPI(UINT32 APICID, UINT8 Vector)
{
    if (!LAPICWaitForIPI())
    {
        return FALSE;
    }

    LAPICWrite(LAPIC_REG_ICR_HIGH, APICID << 24);
    LAPICWrite(LAPIC_REG_ICR_LOW, LAPIC_ICR_DELIVERY_STARTUP | Vector);

    if (!LAPICWaitForIPI())
    {
        return FALSE;
    }

    return TRUE;
}
