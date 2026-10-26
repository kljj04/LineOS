// UserPrograms/Shell/Include/IME.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <LOSLibC/TypeInfo.h>

typedef struct
{
    INT32 Lead;
    INT32 Vowel;
    INT32 Final;
} IME_STATE;

VOID    IMEReset(IME_STATE *State);
UINT32  IMECodepoint(CONST IME_STATE *State);
BOOLEAN IMEFeed(IME_STATE *State, UINT8 Byte, UINT32 *Committed);
VOID    IMEBackspace(IME_STATE *State);
