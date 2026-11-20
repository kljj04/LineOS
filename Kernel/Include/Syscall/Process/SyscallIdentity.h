// Kernel/Include/Syscall/Process/SyscallIdentity.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

INT64 SyscallSetUID(SYSCALL_CONTEXT *Context);
INT64 SyscallSetGID(SYSCALL_CONTEXT *Context);
INT64 SyscallGetUID(SYSCALL_CONTEXT *Context);
