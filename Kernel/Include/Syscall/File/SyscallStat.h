// Kernel/Include/Syscall/File/SyscallStat.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallStat(UINT64 Path, UINT64 Buffer, BOOLEAN Follow);
INT64 SyscallFstat(UINT64 FD, UINT64 Buffer);
