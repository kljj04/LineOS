// Common/Syscall/Device/Input.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

typedef struct
{
    UINT32 X;
    UINT32 Y;
    UINT32 Buttons;
    INT32 Wheel;
} POINTER_INFO;
