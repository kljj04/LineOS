// Kernel/Include/Syscall/Time/SyscallTime.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallClockGetTime(INT64 ClockID, UINT64 UserTime);

INT64 SyscallNanosleep(UINT64 Duration, UINT64 Remaining);
