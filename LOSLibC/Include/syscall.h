// LOSLibC/Include/syscall.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <Syscall/Numbers.h>

UINT64 LineOSSyscall0(UINT64 Number);
UINT64 LineOSSyscall1(UINT64 Number, UINT64 Arg1);
UINT64 LineOSSyscall2(UINT64 Number, UINT64 Arg1, UINT64 Arg2);
UINT64 LineOSSyscall3(UINT64 Number, UINT64 Arg1, UINT64 Arg2, UINT64 Arg3);
INT64  LineOSSyscallResult(UINT64 Result);

UINT64 LineOSSyscall6(UINT64 Number, UINT64 Arg1, UINT64 Arg2, UINT64 Arg3, UINT64 Arg4, UINT64 Arg5, UINT64 Arg6);
