// Kernel/Include/Syscall/Device/SyscallIoctl.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallIoctl(UINT64 FD, UINT64 Request, UINT64 Argument);
