// Kernel/Include/Syscall/File/SyscallStat.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

INT64 SyscallStat(SYSCALL_CONTEXT *Context);
INT64 SyscallFstat(SYSCALL_CONTEXT *Context);

INT64 SyscallNewfstatat(SYSCALL_CONTEXT *Context);
