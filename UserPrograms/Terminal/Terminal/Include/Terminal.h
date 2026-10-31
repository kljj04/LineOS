// UserPrograms/Terminal/Terminal/Include/Terminal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeEngine.h>
#include <Display.h>
#include <ANSI.h>
#include <Window.h>
#include <Syscall/Device/Input.h>
#include <Terminal/Text.h>

#define TERMINAL_MAX_WIDTH          2560U
#define TERMINAL_MAX_HEIGHT         1440U
#define TERMINAL_ALLOCATOR_CAPACITY (8U * 1024U * 1024U)
#define TERMINAL_LINE_HEIGHT        36U
#define TERMINAL_CURSOR_WIDTH       15U
#define TERMINAL_CURSOR_HEIGHT      36U
#define TERMINAL_BACKGROUND         0x001E1E1EU
#define TERMINAL_FOREGROUND         0x00FFFFFFU

typedef struct
{
    UINT32 Advance;
    INT32  Left;
    INT32  Top;
    INT32  Right;
    INT32  Bottom;
} TERMINAL_GLYPH;

typedef struct
{
    UINT32         X;
    TERMINAL_GLYPH Glyph;
} TERMINAL_CELL;

typedef struct
{
    UINT32 Value;
    UINT32 Minimum;
    UINT32 Remaining;
} TERMINAL_UTF8;

extern TERMINAL_GLYPH Glyphs[128];
extern TERMINAL_CELL  LineCells[TERMINAL_MAX_WIDTH];
extern INT32          FramebufferFD;
extern UINT32        *MappedBuffer;
extern UINT32         FrameBuffer[TERMINAL_MAX_WIDTH * TERMINAL_MAX_HEIGHT];
extern UINTN          AllocatorUsed;
extern UINT32         CellAscent;

VOID           *TerminalAllocate(UINTN Size, VOID *Context);
VOID            TerminalFree(VOID *Pointer, VOID *Context);
VOID            TerminalNewLine(DISPLAY_INFO *Info, UINT32 *X, UINT32 *Baseline, UINT32 Background);
BOOLEAN         TerminalDecode(TERMINAL_UTF8 *Decoder, UINT8 Byte, UINT32 *Codepoint, BOOLEAN *Retry);
TRUE_TYPE_FONT *TerminalMeasure(TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback, UINT32 Codepoint, TERMINAL_GLYPH *Glyph);
INT64           TerminalPresent(DISPLAY_INFO *Info, UINT32 X, UINT32 Baseline, BOOLEAN CursorVisible);
INT32           TerminalRun(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback);

VOID TerminalMousePoll(DISPLAY_INFO *Info, UINT32 Baseline, UINT32 LineLength, BOOLEAN Enabled);

VOID    TerminalScrollWheel(INT32 Wheel);
VOID    TerminalScrollReset(VOID);
UINT32 *TerminalScrollView(DISPLAY_INFO *Info);

BOOLEAN TerminalScrollActive(VOID);
BOOLEAN TerminalScrollTick(UINT64 Now);
BOOLEAN TerminalScrollPointer(DISPLAY_INFO *Info, UINT32 X, UINT32 Y, BOOLEAN Pressed);

UINT32                   TerminalDrawGlyph(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, UINT32 Codepoint, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 Advance);
extern LOS_WINDOW        TerminalWindow;
extern TRUE_TYPE_SURFACE TerminalSurface;
BOOLEAN                  TerminalWindowUpdate(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback, UINT32 *X, UINT32 *Baseline, UINT32 LineLength, BOOLEAN Mouse);
VOID                     TerminalMouseEvent(DISPLAY_INFO *Info, UINT32 Baseline, UINT32 LineLength, BOOLEAN Enabled, CONST POINTER_INFO *Pointer);

extern BOOLEAN TerminalWindowFailed;

extern UINT32 TerminalDirtyFirst;
extern UINT32 TerminalDirtyLast;
VOID TerminalDamage(UINT32 First, UINT32 Last);
