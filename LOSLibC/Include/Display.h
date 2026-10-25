// LOSLibC/Include/Display.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

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

INT64 DisplayGetInfo(DISPLAY_INFO *Info);
INT64 DisplayPresent(CONST VOID *Buffer, UINT64 BufferSize);
INT64 DisplayPresentRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, CONST VOID *Buffer);
