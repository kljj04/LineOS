// UserPrograms/Terminal/Terminal/Include/TerminalScreen.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Terminal.h>

#define TERMINAL_SCREEN_ROWS (TERMINAL_MAX_HEIGHT / TERMINAL_LINE_HEIGHT + 1U)

typedef struct
{
    UINT32 Codepoint;
    UINT32 Foreground;
    UINT32 Background;
    UINT32 X;
    UINT32 Advance;
} TERMINAL_TEXT_CELL;

typedef struct
{
    TERMINAL_TEXT_CELL *Cells;
    UINT32              Length;
    UINT32              Background;
    UINT64              Revision;
} TERMINAL_TEXT_LINE;

VOID                TerminalScreenInit(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback);
VOID                TerminalScreenClear(UINT32 Background);
VOID                TerminalScreenNewLine(UINT32 Background);
VOID                TerminalScreenLoad(UINT32 Baseline, UINT32 *Length);
VOID                TerminalScreenErase(UINT32 Baseline, UINT32 Background);
VOID                TerminalScreenPut(UINT32 Baseline, UINT32 Cursor, UINT32 Codepoint, UINT32 X, UINT32 Advance, UINT32 Foreground, UINT32 Background);
VOID                TerminalScreenRender(DISPLAY_INFO *Info);
BOOLEAN             TerminalScreenHealthy(VOID);
UINT32              TerminalScreenRows(VOID);
TERMINAL_TEXT_LINE *TerminalScreenLine(UINT32 Row);
BOOLEAN             TerminalHistorySave(CONST TERMINAL_TEXT_LINE *Line);
VOID                TerminalHistorySelect(VOID);
TERMINAL_TEXT_LINE *TerminalHistoryNext(UINT32 Row);

VOID TerminalScreenResize(DISPLAY_INFO *Info, UINT32 *Baseline);
