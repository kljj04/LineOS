// hpet.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <memory/memory.h>
#include <time/hpet.h>

#define HPET_GENERAL_CAPABILITIES 0x000
#define HPET_GENERAL_CONFIGURATION 0x010
#define HPET_MAIN_COUNTER 0x0F0
#define HPET_ENABLE_CNF 0x001

LINEOS_HPET_INFO HPETInfo;

VOID HPETInit(VOID)
{
    UINT64 capabilities;

    HPETInfo.BaseAddress = HPETGetBaseAddress();
    HPETInfo.CounterClockPeriod = 0;
    HPETInfo.Available = FALSE;

    if (HPETInfo.BaseAddress == 0)
    {
        return;
    }

    capabilities = MMIORead64(HPETInfo.BaseAddress + HPET_GENERAL_CAPABILITIES);
    HPETInfo.CounterClockPeriod = capabilities >> 32;

    if (HPETInfo.CounterClockPeriod == 0)
    {
        return;
    }

    HPETInfo.Available = TRUE;
    HPETEnable();
}

VOID HPETEnable(VOID)
{
    UINT64 config;

    if (!HPETInfo.Available)
    {
        return;
    }

    config = MMIORead64(HPETInfo.BaseAddress + HPET_GENERAL_CONFIGURATION);
    MMIOWrite64(HPETInfo.BaseAddress + HPET_GENERAL_CONFIGURATION, config | HPET_ENABLE_CNF);
}

UINT64 HPETReadCounter(VOID)
{
    if (!HPETInfo.Available)
    {
        return 0;
    }

    return MMIORead64(HPETInfo.BaseAddress + HPET_MAIN_COUNTER);
}

UINT64 HPETMillisecondsToTicks(UINT64 Milliseconds)
{
    if (!HPETInfo.Available || HPETInfo.CounterClockPeriod == 0)
    {
        return 0;
    }

    return (Milliseconds * 1000000000000ULL) / HPETInfo.CounterClockPeriod;
}
