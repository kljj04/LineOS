# Clock, uptime and NTP

[Documentation index](README.md) · [한국어](../ko_KR/Clock.md) · [English](../en_US/Clock.md)

## Time sources

`ClockInit()` initializes UTC from RTC and the calibrated TSC. `ClockGetTime()` returns Unix seconds/nanoseconds; `ClockGetUnixTime()` returns UTC Unix seconds. `ClockGetDateTime()` applies a display offset only. EXT2 timestamps remain UTC.

`ClockGetUptimeMls()` measures HPET elapsed time since clock initialization. NTP steps do not change uptime or the scheduler/network monotonic deadlines.

## Synchronization

`ClockNTPTask` is available but is not registered by the current `KMain()`. When a caller schedules it alongside the network worker and address configuration is ready, the task queries `pool.ntp.org` and repeats 600 seconds (10 minutes) after success. Each attempt has a five-second budget. Failures retain the current clock and back off through 64, 128, 256, 512 and 600 seconds.

`ClockSyncNTP(Server, TimeoutMls)` accepts a hostname or IPv4 string. `ClockGetNTPStatus()` reports busy/last-success state, failures, server address, measured delay, offset and the next monotonic deadline. Calls are serialized and do not send before the scheduled deadline. `CLOCK_NTP_SERVER` and `CLOCK_NTP_PORT` default to `pool.ntp.org` and UDP 123.

Replies must match the request timestamp, server address/port, length, version, mode, stratum, synchronization state and timestamp order. Four timestamps determine delay and offset. The 2036 rollover is unfolded using the era nearest the RTC-derived time. KoD replies defer retries according to the poll interval, with a 24-hour delay for DENY/RSTR.

## Display

The current Kernel.c displays CPU usage and mouse input; it has no ClockDisplayTask or automatic clock/NTP status display. A caller can use `ClockGetDateTime()` with an offset of 540 minutes for KST (UTC+09:00).

## Limits and source

This is a single-server SNTP client requiring successful RTC initialization. Corrections step wall time, which may move backwards. Hardware RTC is not written. Frequency discipline, slewing, multiple-server selection and NTS authentication are not implemented. Reply correlation does not authenticate the server.

Implementation and interfaces are `Kernel/Source/Timer/Clock.c` and `Kernel/Include/Timer/Clock.h`.

Packet and offset calculations follow [RFC 4330](https://www.rfc-editor.org/rfc/rfc4330.html).
