// Kernel/Include/Syscall/File/SyscallDirectory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallMkdir(UINT64 Path, UINT64 Mode);
INT64 SyscallGetDents64(UINT64 FD, UINT64 Buffer, UINT64 Count);
INT64 SyscallChdir(UINT64 Path);
INT64 SyscallGetCwd(UINT64 Buffer, UINT64 Size);
