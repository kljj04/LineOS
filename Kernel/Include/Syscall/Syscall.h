// Kernel/Include/Syscall/Syscall.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Interrupt/IDT.h>
#include <LineOS/TypeInfo.h>

#define SYSCALL_VECTOR 0x80

enum SYSCALL_TYPE
{
    SYSCALL_EXIT,

    SYSCALL_READ,
    SYSCALL_WRITE,
    SYSCALL_OPEN,
    SYSCALL_CLOSE,

    SYSCALL_GETPID,

    SYSCALL_YIELD,

    SYSCALL_DISPLAY_INFO,
    SYSCALL_DISPLAY_PRESENT,
    SYSCALL_DISPLAY_PRESENT_RECT,

    SYSCALL_STREAM_READ,

    SYSCALL_SPAWN,

    SYSCALL_ASSET_PUBLISH,
    SYSCALL_ASSET_ACQUIRE,
    SYSCALL_ASSET_RELEASE
};

UINT64 SyscallDispatch(INTERRUPT_FRAME *Frame);