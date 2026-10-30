// Kernel/Include/Syscall/Time/SyscallFileTime.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallUtimensat(INT64 DirFD, UINT64 Path, UINT64 Times, UINT64 Flags);
