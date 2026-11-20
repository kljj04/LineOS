// Kernel/Include/Syscall/File/SyscallDirectory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

INT64 SyscallMkdir(SYSCALL_CONTEXT *Context);
INT64 SyscallGetDents64(SYSCALL_CONTEXT *Context);
INT64 SyscallChdir(SYSCALL_CONTEXT *Context);
INT64 SyscallGetCwd(SYSCALL_CONTEXT *Context);
