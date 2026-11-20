// Kernel/Include/Syscall/Process/SyscallExit.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

INT64 SyscallExit(SYSCALL_CONTEXT *Context);
VOID  SyscallCheckGroupExit(VOID);
