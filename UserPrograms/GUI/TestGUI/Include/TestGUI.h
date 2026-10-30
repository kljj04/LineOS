// UserPrograms/GUI/TestGUI/Include/TestGUI.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeEngine.h>
#include <Syscall/Device/Input.h>

#define TEST_GUI_ALLOCATOR_SIZE (8U * 1024U * 1024U)

typedef struct
{
    UINT32 X;
    UINT32 Y;
    UINT32 Width;
    UINT32 Height;
} TEST_GUI_RECT;

typedef struct
{
    UINT32 *Pixels;
    UINT32 Width;
    UINT32 Height;
    UINT8 *Work;
    UINTN Used;
    TRUE_TYPE_FONT Font;
    TEST_GUI_RECT Window;
    TEST_GUI_RECT Close;
    TEST_GUI_RECT Button;
    UINT32 Clicks;
    BOOLEAN Hover;
} TEST_GUI;

VOID *TestGUIAllocate(UINTN Size, VOID *Context);
VOID TestGUIFree(VOID *Pointer, VOID *Context);
BOOLEAN TestGUIContains(CONST TEST_GUI_RECT *Rect, CONST POINTER_INFO *Pointer);
VOID TestGUIDraw(TEST_GUI *GUI);
