// Kernel/Include/Syscall/Network/SyscallSocket.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>
#include <Syscall/File/FileInternal.h>

INT64 SyscallSocket(SYSCALL_CONTEXT *Context);
INT64 SyscallBind(SYSCALL_CONTEXT *Context);
INT64 SyscallListen(SYSCALL_CONTEXT *Context);
INT64 SyscallAccept(SYSCALL_CONTEXT *Context);
INT64 SyscallConnect(SYSCALL_CONTEXT *Context);
INT64 SyscallSocketName(SYSCALL_CONTEXT *Context);
INT64 SyscallSendTo(SYSCALL_CONTEXT *Context);
INT64 SyscallRecvFrom(SYSCALL_CONTEXT *Context);
INT64 SyscallShutdown(SYSCALL_CONTEXT *Context);
INT64 SyscallSocketOption(SYSCALL_CONTEXT *Context);
INT64 SyscallSocketPoll(OPEN_FILE *File, UINT16 Events);
