// Kernel/Include/Syscall/File/SyscallPath.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallRename(UINT64 OldPath, UINT64 NewPath);
INT64 SyscallRemove(UINT64 Path);
