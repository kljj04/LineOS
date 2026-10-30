// UserPrograms/Terminal/Terminal/Include/ANSI.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <LOSLibC/TypeInfo.h>

#define TERMINAL_ANSI_PARAMETERS 32U

typedef enum
{
    TERMINAL_ANSI_TEXT,
    TERMINAL_ANSI_ESCAPE,
    TERMINAL_ANSI_CSI,
    TERMINAL_ANSI_OSC,
    TERMINAL_ANSI_OSC_ESCAPE
} TERMINAL_ANSI_STATE;

typedef struct
{
    TERMINAL_ANSI_STATE State;
    UINT32              Parameters[TERMINAL_ANSI_PARAMETERS];
    UINT32              Count;
    UINT32              Foreground;
    UINT32              Background;
    UINT32              DefaultForeground;
    UINT32              DefaultBackground;
    UINT32              ForegroundIndex;
    BOOLEAN             EraseLine;
    BOOLEAN             EraseScreen;
    BOOLEAN             CursorHome;
    BOOLEAN             MouseReporting;
    BOOLEAN             Private;
    UINT32              CursorColumn;
    UINT32              CursorRow;
    BOOLEAN             CursorPosition;
    BOOLEAN             Synchronized;
    BOOLEAN             CursorHidden;
    BOOLEAN             Invalid;
    BOOLEAN             Bold;
    BOOLEAN             Inverse;
} TERMINAL_ANSI;

VOID    TerminalANSIInit(TERMINAL_ANSI *ANSI, UINT32 Foreground, UINT32 Background);
BOOLEAN TerminalANSIConsume(TERMINAL_ANSI *ANSI, UINT8 Byte);
UINT32  TerminalANSIForeground(CONST TERMINAL_ANSI *ANSI);
UINT32  TerminalANSIBackground(CONST TERMINAL_ANSI *ANSI);
