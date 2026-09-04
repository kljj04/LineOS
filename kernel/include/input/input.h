// kernel/include/input/input.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

typedef struct
{
    UINT32  X;
    UINT32  Y;
    BOOLEAN LeftButton;
    BOOLEAN RightButton;
    BOOLEAN MiddleButton;
    INT32   Wheel;
} MOUSE_STATUS;

typedef struct
{
    UINT16  KeyCode;
    BOOLEAN Pressed;
} KEYBOARD_STATUS;

VOID            InputMouseHandler(VOID);
VOID            InputKeyboardHandler(VOID);
MOUSE_STATUS    GetMouseStatus(VOID);
KEYBOARD_STATUS GetKeyboardStatus(VOID);

VOID HideMouseCursor(VOID);
VOID ShowMouseCursor(VOID);
VOID UpdateMouseCursor(VOID);