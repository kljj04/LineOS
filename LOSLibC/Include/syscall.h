// LOSLibC/Include/syscall.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

UINT64 LineOSSyscall0(UINT64 Number);
UINT64 LineOSSyscall1(UINT64 Number, UINT64 Arg1);
UINT64 LineOSSyscall2(UINT64 Number, UINT64 Arg1, UINT64 Arg2);
UINT64 LineOSSyscall3(UINT64 Number, UINT64 Arg1, UINT64 Arg2, UINT64 Arg3);