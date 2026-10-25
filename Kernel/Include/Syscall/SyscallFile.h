// Kernel/Include/Syscall/SyscallFile.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallOpen(UINT64 Path, UINT64 Flags);
INT64 SyscallRead(UINT64 FD, UINT64 Buffer, UINT64 Count);
INT64 SyscallClose(UINT64 FD);
