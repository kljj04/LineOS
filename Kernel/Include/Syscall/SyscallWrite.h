// Kernel/Include/Syscall/SyscallWrite.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallWrite(UINT64 FD, CONST CHAR8 *Buffer, UINT64 Count);