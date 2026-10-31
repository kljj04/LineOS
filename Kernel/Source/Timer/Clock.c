// Kernel/Source/Timer/Clock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Timer/Clock.h>
#include <Timer/RTC.h>
#include <Timer/TSC.h>
#include <Arch/AMD64/CPU.h>
#include <Arch/AMD64/SpinLock.h>
#include <Timer/HPET.h>
#include <Network/Network.h>
#include <Scheduler/Task.h>
#include <Debug/Debug.h>

#define CLOCK_NS_PER_SECOND   1000000000ULL
#define CLOCK_SECONDS_PER_DAY 86400ULL

#define CLOCK_NTP_EPOCH 2208988800ULL
#define CLOCK_NTP_ERA   4294967296ULL

STATIC SPIN_LOCK        ClockLock;
STATIC UINT64           ClockBaseNanoseconds;
STATIC UINT64           ClockBootCounter;
STATIC UINT64           ClockBootFrequency;
STATIC UINT32           ClockNTPBusy;
STATIC CLOCK_NTP_STATUS ClockNTPStatus;
STATIC UINT64           ClockBaseUnix;
STATIC UINT64           ClockBaseTSC;
STATIC UINT64           ClockFrequency;
STATIC BOOLEAN          ClockReady = FALSE;

STATIC BOOLEAN ClockIsLeapYear(UINT32 Year)
{
    return (Year % 4 == 0) && ((Year % 100 != 0) || (Year % 400 == 0));
}

STATIC UINT32 ClockDaysInMonth(UINT32 Year, UINT32 Month)
{
    STATIC CONST UINT8 Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (Month == 2 && ClockIsLeapYear(Year))
    {
        return 29;
    }

    return Days[Month - 1];
}

STATIC BOOLEAN ClockToUnixTime(CONST RTC_TIME *Time, UINT64 *Seconds)
{
    UINT64 Days = 0;
    UINT32 Year;
    UINT32 Month;

    if (Time->Year < 1970 || Time->Month < 1 || Time->Month > 12 || Time->Hour > 23 || Time->Minute > 59 || Time->Second > 59)
    {
        return FALSE;
    }

    if (Time->Day < 1 || Time->Day > ClockDaysInMonth(Time->Year, Time->Month))
    {
        return FALSE;
    }

    for (Year = 1970; Year < Time->Year; Year++)
    {
        Days += ClockIsLeapYear(Year) ? 366 : 365;
    }

    for (Month = 1; Month < Time->Month; Month++)
    {
        Days += ClockDaysInMonth(Time->Year, Month);
    }

    Days += Time->Day - 1;

    *Seconds = Days * CLOCK_SECONDS_PER_DAY + (UINT64) Time->Hour * 3600 + (UINT64) Time->Minute * 60 + Time->Second;

    return TRUE;
}

BOOLEAN ClockInit(VOID)
{
    RTC_TIME Time;
    UINT64   Before;
    UINT64   After;
    UINT64   UnixTime;
    UINT64   Frequency;

    SpinLockInit(&ClockLock);
    ClockBootCounter = HPETReadCounter();
    ClockBootFrequency = HPETGetFrequency();
    ClockBaseNanoseconds = 0;
    ClockReady = FALSE;

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return FALSE;
    }

    Before = RDTSC();

    if (!RTCReadTime(&Time))
    {
        return FALSE;
    }

    After = RDTSC();

    if (!ClockToUnixTime(&Time, &UnixTime))
    {
        return FALSE;
    }

    ClockBaseUnix = UnixTime;
    ClockBaseTSC = Before + (After - Before) / 2;
    ClockFrequency = Frequency;
    ClockReady = TRUE;

    return TRUE;
}

STATIC BOOLEAN ClockReadTimeLocked(CLOCK_TIME *Time, UINT64 *Counter)
{
    if (!ClockReady || ClockFrequency == 0)
    {
        return FALSE;
    }

    UINT64 Now = RDTSC();

    if (Now < ClockBaseTSC)
    {
        return FALSE;
    }

    UINT64 Nanoseconds = (UINT64) ((UINT128) (Now - ClockBaseTSC) * CLOCK_NS_PER_SECOND / ClockFrequency) + ClockBaseNanoseconds;

    Time->Seconds = ClockBaseUnix + (UINT64) (Nanoseconds / CLOCK_NS_PER_SECOND);
    Time->Nanoseconds = (UINT32) (Nanoseconds % CLOCK_NS_PER_SECOND);

    if (Counter != NULL)
    {
        *Counter = Now;
    }

    return TRUE;
}

BOOLEAN ClockGetTime(CLOCK_TIME *Time)
{
    if (Time == NULL)
    {
        return FALSE;
    }

    UINT64  Flags = SpinLockAcquireIRQSave(&ClockLock);
    BOOLEAN Result = ClockReadTimeLocked(Time, NULL);

    SpinLockReleaseIRQRestore(&ClockLock, Flags);

    return Result;
}

UINT64 ClockGetUptimeMls(VOID)
{
    UINT64 Now = HPETReadCounter();

    if (ClockBootFrequency == 0 || Now < ClockBootCounter)
    {
        return 0;
    }

    return (UINT64) ((UINT128) (Now - ClockBootCounter) * 1000 / ClockBootFrequency);
}

VOID ClockGetNTPStatus(CLOCK_NTP_STATUS *Status)
{
    if (Status == NULL)
    {
        return;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&ClockLock);

    *Status = ClockNTPStatus;
    SpinLockReleaseIRQRestore(&ClockLock, Flags);
}

UINT64 ClockGetUnixTime(VOID)
{
    CLOCK_TIME Time;

    if (!ClockGetTime(&Time))
    {
        return 0;
    }

    return Time.Seconds;
}

BOOLEAN ClockGetDateTime(CLOCK_DATE_TIME *DateTime, INT32 UTCOffsetMinutes)
{
    CLOCK_TIME Time;
    INT64      Seconds;
    INT64      Days;
    INT64      Remaining;
    UINT32     Year = 1970;
    UINT32     Month = 1;
    UINT32     DaysInYear;
    UINT32     DaysInMonth;

    if (DateTime == NULL || !ClockGetTime(&Time))
    {
        return FALSE;
    }

    if (UTCOffsetMinutes < -1440 || UTCOffsetMinutes > 1440)
    {
        return FALSE;
    }

    Seconds = (INT64) Time.Seconds + (INT64) UTCOffsetMinutes * 60;

    if (Seconds < 0)
    {
        return FALSE;
    }

    Days = Seconds / 86400;
    Remaining = Seconds % 86400;

    while (TRUE)
    {
        DaysInYear = ClockIsLeapYear(Year) ? 366 : 365;

        if (Days < DaysInYear)
        {
            break;
        }

        Days -= DaysInYear;
        Year++;

        if (Year > UINT16_MAX)
        {
            return FALSE;
        }
    }

    while (TRUE)
    {
        DaysInMonth = ClockDaysInMonth(Year, Month);

        if (Days < DaysInMonth)
        {
            break;
        }

        Days -= DaysInMonth;
        Month++;
    }

    DateTime->Year = (UINT16) Year;
    DateTime->Month = (UINT8) Month;
    DateTime->Day = (UINT8) (Days + 1);
    DateTime->Hour = (UINT8) (Remaining / 3600);
    DateTime->Minute = (UINT8) ((Remaining % 3600) / 60);
    DateTime->Second = (UINT8) (Remaining % 60);

    return TRUE;
}

STATIC UINT64 ClockNTPRead64(CONST UINT8 *Data)
{
    UINT64 Value = 0;

    for (UINT32 Index = 0; Index < 8; Index++)
    {
        Value = (Value << 8) | Data[Index];
    }

    return Value;
}

STATIC VOID ClockNTPWrite64(UINT8 *Data, UINT64 Value)
{
    for (UINT32 Index = 0; Index < 8; Index++)
    {
        Data[7 - Index] = (UINT8) Value;
        Value >>= 8;
    }
}

STATIC UINT32 ClockNTPRemaining(UINT64 Start, UINT32 TimeoutMls)
{
    UINT64 Elapsed = ClockGetUptimeMls() - Start;

    return Elapsed >= TimeoutMls ? 0 : TimeoutMls - (UINT32) Elapsed;
}

STATIC BOOLEAN ClockNTPDecode(UINT64 Timestamp, UINT64 Hint, INT128 *Nanoseconds)
{
    UINT64 Reference = Hint + CLOCK_NTP_EPOCH;
    UINT64 Seconds = (Reference & ~(CLOCK_NTP_ERA - 1)) | (Timestamp >> 32);

    if (Seconds + CLOCK_NTP_ERA / 2 < Reference)
    {
        Seconds += CLOCK_NTP_ERA;
    }
    else if (Seconds > Reference + CLOCK_NTP_ERA / 2)
    {
        if (Seconds < CLOCK_NTP_ERA)
        {
            return FALSE;
        }

        Seconds -= CLOCK_NTP_ERA;
    }

    if (Seconds < CLOCK_NTP_EPOCH)
    {
        return FALSE;
    }

    *Nanoseconds = (INT128) (Seconds - CLOCK_NTP_EPOCH) * CLOCK_NS_PER_SECOND + ((UINT128) (UINT32) Timestamp * CLOCK_NS_PER_SECOND >> 32);

    return TRUE;
}

STATIC BOOLEAN ClockNTPApply(CONST UINT8 *Packet, CONST CLOCK_TIME *Sent, CONST CLOCK_TIME *Received, UINT64 ReceivedCounter, UINT32 TimeoutMls)
{
    INT128 ServerReceive;
    INT128 ServerTransmit;
    INT128 LocalSend = (INT128) Sent->Seconds * CLOCK_NS_PER_SECOND + Sent->Nanoseconds;
    INT128 LocalReceive = (INT128) Received->Seconds * CLOCK_NS_PER_SECOND + Received->Nanoseconds;

    if (ClockNTPRead64(Packet + 32) == 0 || ClockNTPRead64(Packet + 40) == 0 || ((ClockNTPRead64(Packet + 40) - ClockNTPRead64(Packet + 32)) >> 63) != 0 || !ClockNTPDecode(ClockNTPRead64(Packet + 32), Sent->Seconds, &ServerReceive) || !ClockNTPDecode(ClockNTPRead64(Packet + 40), Sent->Seconds, &ServerTransmit))
    {
        return FALSE;
    }

    INT128 Processing = ServerTransmit - ServerReceive;
    INT128 Elapsed = LocalReceive - LocalSend;
    INT128 RoundTrip = Elapsed - Processing;
    INT128 Sum = (ServerReceive - LocalSend) + (ServerTransmit - LocalReceive);
    INT64  Offset = Sum < INT64_MIN || Sum > INT64_MAX ? 0 : (INT64) Sum / 2;
    INT128 Corrected = LocalReceive + Offset;

    if (Processing < 0 || Elapsed < 0 || Elapsed > (INT128) TimeoutMls * 1000000 || RoundTrip < -1000000 || Corrected < 0 || Sum < INT64_MIN || Sum > INT64_MAX)
    {
        return FALSE;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&ClockLock);
    UINT64 Now = RDTSC();

    if (!ClockReady || ClockFrequency == 0 || Now < ReceivedCounter)
    {
        SpinLockReleaseIRQRestore(&ClockLock, Flags);

        return FALSE;
    }

    Corrected += (UINT128) (Now - ReceivedCounter) * CLOCK_NS_PER_SECOND / ClockFrequency;
    ClockBaseUnix = (UINT64) ((UINT128) Corrected / CLOCK_NS_PER_SECOND);
    ClockBaseNanoseconds = (UINT64) ((UINT128) Corrected - (UINT128) ClockBaseUnix * CLOCK_NS_PER_SECOND);
    ClockBaseTSC = Now;
    ClockNTPStatus.Synchronized = TRUE;
    ClockNTPStatus.LastSyncUptimeMls = ClockGetUptimeMls();
    ClockNTPStatus.OffsetNanoseconds = (INT64) Offset;
    ClockNTPStatus.RoundTripMls = RoundTrip < 0 ? 0 : (UINT32) ((UINT64) RoundTrip / 1000000);
    SpinLockReleaseIRQRestore(&ClockLock, Flags);

    return TRUE;
}

STATIC BOOLEAN ClockNTPExchange(CONST CHAR8 *Server, UINT32 TimeoutMls, UINT64 *RetryDelay)
{
    UINT64     Start = ClockGetUptimeMls();
    UINT32     Address;
    UINT32     Socket;
    UINT32     Length;
    UINT32     Source;
    UINT16     Port;
    UINT8      Request[48] = {0};
    UINT8      Reply[NETWORK_UDP_MAX_PAYLOAD];
    CLOCK_TIME Sent;
    CLOCK_TIME Received;
    UINT64     ReceivedCounter;
    BOOLEAN    Result = FALSE;

    if (Server == NULL || TimeoutMls == 0 || ClockBootFrequency == 0 || !ClockGetTime(&Sent) || !NetworkDNSResolve(Server, &Address, TimeoutMls) || !NetworkUDPOpen(0, &Socket))
    {
        return FALSE;
    }

    Request[0] = 0x23;
    Request[2] = 10;
    Request[3] = (UINT8) -20;

    if (!ClockGetTime(&Sent))
    {
        NetworkUDPClose(Socket);

        return FALSE;
    }

    UINT64 Timestamp = ((Sent.Seconds + CLOCK_NTP_EPOCH) << 32) | (UINT32) (((UINT128) Sent.Nanoseconds << 32) / CLOCK_NS_PER_SECOND);

    ClockNTPWrite64(Request + 40, Timestamp);

    UINT32 Remaining = ClockNTPRemaining(Start, TimeoutMls);

    if (Remaining != 0 && NetworkUDPSendTo(Socket, Address, CLOCK_NTP_PORT, Request, sizeof(Request), Remaining))
    {
        while ((Remaining = ClockNTPRemaining(Start, TimeoutMls)) != 0)
        {
            if (!NetworkUDPReceiveFrom(Socket, Reply, sizeof(Reply), &Length, &Source, &Port, Remaining))
            {
                break;
            }

            UINT64  Flags = SpinLockAcquireIRQSave(&ClockLock);
            BOOLEAN Read = ClockReadTimeLocked(&Received, &ReceivedCounter);

            SpinLockReleaseIRQRestore(&ClockLock, Flags);

            if (!Read || Source != Address || Port != CLOCK_NTP_PORT || Length != 48 || (Reply[0] & 7) != 4 || ((Reply[0] >> 3) & 7) < 3 || ((Reply[0] >> 3) & 7) > 4 || ClockNTPRead64(Reply + 24) != Timestamp)
            {
                continue;
            }

            if (Reply[1] == 0)
            {
                INT8   Poll = (INT8) Reply[2];
                UINT64 Delay = Poll >= 4 && Poll <= 17 ? (1ULL << Poll) * 1000 : CLOCK_NTP_INTERVAL_MLS;

                *RetryDelay = Delay > CLOCK_NTP_INTERVAL_MLS ? Delay : CLOCK_NTP_INTERVAL_MLS;

                if ((Reply[12] == 'D' && Reply[13] == 'E' && Reply[14] == 'N' && Reply[15] == 'Y') || (Reply[12] == 'R' && Reply[13] == 'S' && Reply[14] == 'T' && Reply[15] == 'R'))
                {
                    *RetryDelay = 86400000;
                }

                break;
            }

            if ((Reply[0] >> 6) == 3 || Reply[1] > 15)
            {
                continue;
            }

            if (ClockNTPApply(Reply, &Sent, &Received, ReceivedCounter, TimeoutMls))
            {
                Flags = SpinLockAcquireIRQSave(&ClockLock);
                ClockNTPStatus.ServerAddress = Address;
                SpinLockReleaseIRQRestore(&ClockLock, Flags);
                Result = TRUE;
                break;
            }
        }
    }

    NetworkUDPClose(Socket);

    return Result;
}

BOOLEAN ClockSyncNTP(CONST CHAR8 *Server, UINT32 TimeoutMls)
{
    UINT32 Expected = 0;

    if (!ATOMIC_COMPARE_EXCHANGE(&ClockNTPBusy, &Expected, 1, FALSE, ATOMIC_ACQUIRE, ATOMIC_RELAXED))
    {
        return FALSE;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&ClockLock);

    if (ClockGetUptimeMls() < ClockNTPStatus.NextSyncUptimeMls)
    {
        SpinLockReleaseIRQRestore(&ClockLock, Flags);
        ATOMIC_STORE(&ClockNTPBusy, 0, ATOMIC_RELEASE);

        return FALSE;
    }

    ClockNTPStatus.Busy = TRUE;
    SpinLockReleaseIRQRestore(&ClockLock, Flags);

    UINT64  RetryDelay = 0;
    BOOLEAN Result = ClockNTPExchange(Server, TimeoutMls, &RetryDelay);

    Flags = SpinLockAcquireIRQSave(&ClockLock);
    ClockNTPStatus.Busy = FALSE;

    if (Result)
    {
        ClockNTPStatus.Failures = 0;
        RetryDelay = CLOCK_NTP_INTERVAL_MLS;
    }
    else
    {
        ClockNTPStatus.Failures = ClockNTPStatus.Failures < 16 ? ClockNTPStatus.Failures + 1 : 16;

        UINT64 Backoff = 64000ULL << (ClockNTPStatus.Failures < 5 ? ClockNTPStatus.Failures - 1 : 4);

        Backoff = Backoff > CLOCK_NTP_INTERVAL_MLS ? CLOCK_NTP_INTERVAL_MLS : Backoff;
        RetryDelay = RetryDelay > Backoff ? RetryDelay : Backoff;
    }

    ClockNTPStatus.NextSyncUptimeMls = ClockGetUptimeMls() + RetryDelay;
    SpinLockReleaseIRQRestore(&ClockLock, Flags);
    ATOMIC_STORE(&ClockNTPBusy, 0, ATOMIC_RELEASE);

    return Result;
}

VOID ClockNTPTask(VOID)
{
    for (;;)
    {
        NETWORK_CONFIG   Config;
        CLOCK_NTP_STATUS Status;

        ClockGetNTPStatus(&Status);

        if (NetworkGetConfig(&Config) && ClockGetUptimeMls() >= Status.NextSyncUptimeMls)
        {
            BOOLEAN Result = ClockSyncNTP((CONST CHAR8 *) CLOCK_NTP_SERVER, 5000);

            DebugWriteLine(Result ? "[CLOCK] NTP synchronized; next update in 600 seconds" : "[CLOCK] NTP unavailable; retaining current clock");
        }

        TaskSleepMls(1000);
    }
}

BOOLEAN ClockGetMonotonicTime(CLOCK_TIME *Time)
{
    if (Time == NULL || ClockBootFrequency == 0)
    {
        return FALSE;
    }

    UINT64 Now = HPETReadCounter();

    if (Now < ClockBootCounter)
    {
        return FALSE;
    }

    UINT128 Nanoseconds = (UINT128) (Now - ClockBootCounter) * CLOCK_NS_PER_SECOND / ClockBootFrequency;

    Time->Seconds = (UINT64) (Nanoseconds / CLOCK_NS_PER_SECOND);
    Time->Nanoseconds = (UINT32) (Nanoseconds % CLOCK_NS_PER_SECOND);

    return TRUE;
}
