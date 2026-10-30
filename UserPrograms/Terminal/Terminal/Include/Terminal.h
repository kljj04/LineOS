// UserPrograms/Terminal/Terminal/Include/Terminal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeEngine.h>
#include <Display.h>
#include <ANSI.h>

#define TERMINAL_MAX_WIDTH          2560U
#define TERMINAL_MAX_HEIGHT         1440U
#define TERMINAL_ALLOCATOR_CAPACITY (8U * 1024U * 1024U)
#define TERMINAL_FONT_HEIGHT        36U
#define TERMINAL_LINE_HEIGHT        40U
#define TERMINAL_CURSOR_WIDTH       15U
#define TERMINAL_CURSOR_HEIGHT      40U
#define TERMINAL_MARGIN             8U
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
extern TERMINAL_CELL LineCells[TERMINAL_MAX_WIDTH];
extern INT32 FramebufferFD;
extern UINT32 *MappedBuffer;
extern UINT32 FrameBuffer[TERMINAL_MAX_WIDTH * TERMINAL_MAX_HEIGHT];
extern UINTN AllocatorUsed;
extern UINT32 CellAscent;

VOID *TerminalAllocate(UINTN Size, VOID *Context);
VOID TerminalFree(VOID *Pointer, VOID *Context);
VOID TerminalNewLine(DISPLAY_INFO *Info, UINT32 *X, UINT32 *Baseline, UINT32 Background);
VOID TerminalErase(DISPLAY_INFO *Info, TERMINAL_GLYPH *Glyph, UINT32 X, UINT32 Baseline, UINT32 Background);
BOOLEAN TerminalDecode(TERMINAL_UTF8 *Decoder, UINT8 Byte, UINT32 *Codepoint, BOOLEAN *Retry);
TRUE_TYPE_FONT *TerminalMeasure(TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback, UINT32 Codepoint, TERMINAL_GLYPH *Glyph);
INT64 TerminalPresent(DISPLAY_INFO *Info, UINT32 X, UINT32 Baseline, BOOLEAN CursorVisible);
INT32 TerminalRun(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback);
