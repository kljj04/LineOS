// Kernel/Include/Syscall/Device/SyscallDisplay.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

#define DISPLAY_FORMAT_BGRA8888 1U

typedef struct
{
    UINT32 Width;
    UINT32 Height;
    UINT32 Pitch;
    UINT32 Format;
} DISPLAY_INFO;

typedef struct
{
    UINT32 X;
    UINT32 Y;
    UINT32 Width;
    UINT32 Height;
    UINT64 Buffer;
} DISPLAY_RECT;

INT64 SyscallDisplayInfo(SYSCALL_CONTEXT *Context);
INT64 SyscallDisplayPresent(SYSCALL_CONTEXT *Context);
INT64 SyscallDisplayPresentRect(SYSCALL_CONTEXT *Context);