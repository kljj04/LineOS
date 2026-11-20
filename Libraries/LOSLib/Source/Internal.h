// Libraries/LOSLib/Source/Internal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <LOSLib/TypeInfo.h>
#include <Syscall/Numbers.h>

__attribute__((visibility("hidden"))) UINT64 LOSSyscall1(UINT64 Number, UINT64 Arg1);
__attribute__((visibility("hidden"))) UINT64 LOSSyscall2(UINT64 Number, UINT64 Arg1, UINT64 Arg2);
__attribute__((visibility("hidden"))) UINT64 LOSSyscall3(UINT64 Number, UINT64 Arg1, UINT64 Arg2, UINT64 Arg3);
__attribute__((visibility("hidden"))) INT64  LOSSyscallResult(UINT64 Result);
