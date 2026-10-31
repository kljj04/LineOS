// UserPrograms/System/lwm/Include/Lwm.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <stddef.h>
#include <TrueType/TrueTypeEngine.h>
#include <Syscall/Device/Input.h>
#include <Asset.h>
#include <Window.h>

#define LWM_WINDOW_LIMIT   16U
#define LWM_FONT_WORK_SIZE (8U * 1024U * 1024U)
#define LWM_PANEL_HEIGHT   48U
#define LWM_DOCK_HEIGHT    64U
#define LWM_TITLE_HEIGHT   44U
#define LWM_MIN_WIDTH      320U
#define LWM_MIN_HEIGHT     300U

typedef struct
{
    INT32 X;
    INT32 Y;
    UINT32 Width;
    UINT32 Height;
} LWM_RECT;

typedef struct
{
    LWM_RECT Rect;
    LWM_RECT Restore;
    UINT32 *Pixels;
    UINT32 Stride;
    UINT32 CapacityHeight;
    UINT64 Bytes;
    UINT32 Clicks;
    BOOLEAN Hover;
    BOOLEAN Dirty;
    UINT64 Endpoint;
    UINT32 *ClientPixels;
    UINT32 *PendingPixels;
    UINT32 ClientWidth;
    UINT32 ClientHeight;
    UINT64 ClientBytes;
    UINT32 ClientFlags;
    BOOLEAN Minimized;
    BOOLEAN Maximized;
    BOOLEAN HasFrame;
    UINT32 PendingFirst;
    UINT32 PendingLast;
    CHAR8 Title[64];
    LWM_RECT PaintedRect;
    BOOLEAN PaintTracked;
    BOOLEAN PaintedVisible;
} LWM_WINDOW;

typedef struct
{
    INT32 TTY;
    INT32 FB;
    UINT32 Width;
    UINT32 Height;
    UINT64 Bytes;
    UINT32 *Mapped;
    UINT32 *Frame;
    UINT32 *Desktop;
    UINT8 *FontWork;
    UINTN FontUsed;
    LOS_ASSET FontAsset;
    LOS_ASSET FallbackAsset;
    TRUE_TYPE_FONT Font;
    TRUE_TYPE_FONT Fallback;
    TRUE_TYPE_SURFACE Surface;
    TTF_ALLOCATOR Allocator;
    LWM_WINDOW Windows[LWM_WINDOW_LIMIT];
    LWM_WINDOW Shell;
    BOOLEAN ShellFocused;
    UINT32 WindowCount;
    UINT32 PreviousButtons;
    INT32 Grab;
    INT32 GrabX;
    INT32 GrabY;
    BOOLEAN Resizing;
    UINT32 ResizeEdges;
    LWM_RECT GrabRect;
    BOOLEAN InitialShell;
    UINT64 PointerCapture;
    BOOLEAN Graphics;
    BOOLEAN Dirty;
    BOOLEAN Running;
    UINT64 Endpoint;
    UINT8 *MessageBuffer;
    POINTER_INFO LastPointer;
    BOOLEAN DamageRows[2160];
    UINT32 DamageLeft;
    UINT32 DamageRight;
    BOOLEAN FrameReady;
    UINT64 DockState;
    UINT32 *PresentBuffer;
} LWM;

BOOLEAN LwmOpen(LWM *Manager);
VOID LwmClose(LWM *Manager);
VOID *LwmAllocate(UINTN Size, VOID *Context);
VOID LwmFree(VOID *Pointer, VOID *Context);
VOID LwmFill(LWM *Manager, LWM_RECT Rect, UINT32 Color);
VOID LwmText(LWM *Manager, CONST char *Text, UINT32 X, UINT32 Baseline, UINT32 Size, UINT32 Color, UINT32 Right);
VOID LwmDesktop(LWM *Manager);
INT32 LwmCompose(LWM *Manager);
BOOLEAN LwmCreateWindow(LWM *Manager);
VOID LwmPaintWindow(LWM *Manager, LWM_WINDOW *Window, BOOLEAN Focused);
VOID LwmPointer(LWM *Manager, CONST POINTER_INFO *Pointer);

INT32 LwmIPC(LWM *Manager);
BOOLEAN LwmClientEvent(LWM *Manager, LWM_WINDOW *Window, UINT32 Type, CONST POINTER_INFO *Pointer, UINT32 Key);
VOID LwmRemoveWindow(LWM *Manager, UINT32 Index);
BOOLEAN LwmKeyboard(LWM *Manager, UINT8 Key);
VOID LwmDock(LWM *Manager);
BOOLEAN LwmDockPointer(LWM *Manager, CONST POINTER_INFO *Pointer, BOOLEAN Clicked);

VOID LwmMaximize(LWM *Manager, LWM_WINDOW *Window);

VOID LwmRemoveShell(LWM *Manager);

VOID LwmDamage(LWM *Manager, LWM_RECT Rect);
UINT32 LwmRoundInset(UINT32 Row, UINT32 Height, UINT32 Radius);