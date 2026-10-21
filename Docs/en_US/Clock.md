# Clock, uptime and NTP

[Documentation index](README.md) · [한국어](../ko_KR/Clock.md) · [English](../en_US/Clock.md)

## Time sources

`ClockInit()` initializes UTC from RTC and the calibrated TSC. `ClockGetTime()` returns Unix seconds/nanoseconds; `ClockGetUnixTime()` returns UTC Unix seconds. `ClockGetDateTime()` applies a display offset only. EXT2 timestamps remain UTC.

`ClockGetUptimeMls()` measures HPET elapsed time since clock initialization. NTP steps do not change uptime or the scheduler/network monotonic deadlines.

## Synchronization

`KMain()` registers `ClockNTPTask` separately from the network worker. Once address configuration is ready, the task queries `pool.ntp.org` and repeats 600 seconds (10 minutes) after success. Each attempt has a five-second budget. Failures retain the current clock and back off through 64, 128, 256, 512 and 600 seconds.

`ClockSyncNTP(Server, TimeoutMls)` accepts a hostname or IPv4 string. `ClockGetNTPStatus()` reports busy/last-success state, failures, server address, measured delay, offset and the next monotonic deadline. Calls are serialized and do not send before the scheduled deadline. `CLOCK_NTP_SERVER` and `CLOCK_NTP_PORT` default to `pool.ntp.org` and UDP 123.

Replies must match the request timestamp, server address/port, length, version, mode, stratum, synchronization state and timestamp order. Four timestamps determine delay and offset. The 2036 rollover is unfolded using the era nearest the RTC-derived time. KoD replies defer retries according to the poll interval, with a 24-hour delay for DENY/RSTR.

## Display

`ClockDisplayTask()` in `Kernel/Kernel.c` refreshes uptime, the KST date/time (UTC+09:00) and NTP status once per second. QHD uses 72px for current time, 64px for uptime and 56px for status, with smaller sizes on narrower screens. Before synchronization it displays RTC-derived time.

## Limits and checks

This is a single-server SNTP client requiring successful RTC initialization. Corrections step wall time, which may move backwards. Hardware RTC is not written. Frequency discipline, slewing, multiple-server selection and NTS authentication are not implemented. Reply correlation does not authenticate the server.

Implementation and interfaces are `Kernel/Source/Timer/Clock.c` and `Kernel/Include/Timer/Clock.h`. Host checks cover malformed replies, forward/backward corrections, the 600-second cadence with simulated time, retries/KoD, era rollover and independent uptime. QEMU checks exercise a UDP test server returning 2037 dates, external NTP and screenshots. Simulated cadence checks are distinct from waiting ten real minutes.

```bash
python3 Tests/NetworkStack/RunHost.py
python3 Tests/NetworkStack/Run.py --cpus 16 --scheduled --clock
python3 Tests/NetworkStack/Run.py --cpus 16 --scheduled --clock --external-ntp --production
```

Screenshots require Python Pillow. Tests use isolated boot files and copied firmware variables. External tests depend on UDP 123 reachability.

Packet and offset calculations follow [RFC 4330](https://www.rfc-editor.org/rfc/rfc4330.html).
