// Kernel/Include/Syscall/File/SyscallPath.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

INT64 SyscallRename(SYSCALL_CONTEXT *Context);
INT64 SyscallRemove(SYSCALL_CONTEXT *Context);
