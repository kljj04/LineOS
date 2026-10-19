// Kernel/Source/Timer/RTC.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Timer/RTC.h>
#include <Arch/AMD64/CPU.h>
#include <Arch/AMD64/SpinLock.h>

STATIC SPIN_LOCK RTCLock;

STATIC UINT8 RTCReadRegister(UINT8 Register)
{
    OUTB(RTC_PORT_INDEX, Register | 0x80);

    return INB(RTC_PORT_DATA);
}

STATIC UINT8 RTCBCDToBinary(UINT8 Value)
{
    return (Value & 0x0F) + ((Value >> 4) * 10);
}

STATIC BOOLEAN RTCReadRaw(RTC_TIME *Time, UINT8 *Status)
{
    UINT8 Year;

    if (RTCReadRegister(RTC_REG_A) & RTC_UIP)
    {
        return FALSE;
    }

    Time->Second = RTCReadRegister(RTC_REG_SECOND);

    Time->Minute = RTCReadRegister(RTC_REG_MINUTE);

    Time->Hour = RTCReadRegister(RTC_REG_HOUR);

    Time->Day = RTCReadRegister(RTC_REG_DAY);

    Time->Month = RTCReadRegister(RTC_REG_MONTH);

    Year = RTCReadRegister(RTC_REG_YEAR);

    *Status = RTCReadRegister(RTC_REG_B);

    Time->Year = Year;

    return !(RTCReadRegister(RTC_REG_A) & RTC_UIP);
}

BOOLEAN RTCReadTime(RTC_TIME *Time)
{
    RTC_TIME First;
    RTC_TIME Second;
    UINT8    FirstStatus;
    UINT8    SecondStatus;
    UINT8    Hour;
    UINT8    PM;
    UINT32   Retry;
    UINT64   Flags;
    BOOLEAN  Success = FALSE;

    if (Time == NULL)
    {
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&RTCLock);

    for (Retry = 0; Retry < RTC_MAX_RETRIES; Retry++)
    {
        if (!RTCReadRaw(&First, &FirstStatus))
        {
            continue;
        }

        if (!RTCReadRaw(&Second, &SecondStatus))
        {
            continue;
        }

        if (FirstStatus != SecondStatus)
        {
            continue;
        }

        if (First.Year != Second.Year || First.Month != Second.Month || First.Day != Second.Day || First.Hour != Second.Hour || First.Minute != Second.Minute || First.Second != Second.Second)
        {
            continue;
        }

        Success = TRUE;
        break;
    }

    SpinLockReleaseIRQRestore(&RTCLock, Flags);

    if (!Success)
    {
        return FALSE;
    }

    Hour = Second.Hour;
    PM = Hour & RTC_PM;
    Hour &= ~RTC_PM;

    if (!(SecondStatus & RTC_BINARY))
    {
        Second.Second = RTCBCDToBinary(Second.Second);

        Second.Minute = RTCBCDToBinary(Second.Minute);

        Hour = RTCBCDToBinary(Hour);

        Second.Day = RTCBCDToBinary(Second.Day);

        Second.Month = RTCBCDToBinary(Second.Month);

        Second.Year = RTCBCDToBinary((UINT8) Second.Year);
    }

    if (!(SecondStatus & RTC_24_HOUR))
    {
        if (Hour < 1 || Hour > 12)
        {
            return FALSE;
        }

        Hour %= 12;

        if (PM)
        {
            Hour += 12;
        }
    }

    Second.Hour = Hour;

    if (Second.Year >= 70)
    {
        Second.Year += 1900;
    }
    else
    {
        Second.Year += 2000;
    }

    if (Second.Month < 1 || Second.Month > 12 || Second.Day < 1 || Second.Day > 31 || Second.Hour > 23 || Second.Minute > 59 || Second.Second > 59)
    {
        return FALSE;
    }

    *Time = Second;

    return TRUE;
}