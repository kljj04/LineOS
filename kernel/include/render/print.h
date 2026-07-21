// print.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

#define BLACK         0x000000
#define BLUE          0x0000AA
#define GREEN         0x00AA00
#define CYAN          0x00AAAA
#define RED           0xAA0000
#define MAGENTA       0xAA00AA
#define BROWN         0xAA5500
#define LIGHT_GRAY    0xAAAAAA

#define DARK_GRAY     0x555555
#define LIGHT_BLUE    0x5555FF
#define LIGHT_GREEN   0x55FF55
#define LIGHT_CYAN    0x55FFFF
#define LIGHT_RED     0xFF5555
#define LIGHT_MAGENTA 0xFF55FF
#define YELLOW        0xFFFF55
#define WHITE         0xFFFFFF

#define CODE_BACKGROUND 0x1E1E1E
#define CODE_TEXT       0xCDD6F4
#define CODE_KEYWORD    0xCBA6F7
#define CODE_TYPE       0x89B4FA
#define CODE_FUNCTION   0xF9E2AF
#define CODE_STRING     0xA6E3A1
#define CODE_NUMBER     0xFAB387
#define CODE_COMMENT    0x6C7086
#define CODE_MACRO      0xF38BA8
#define CODE_SYMBOL     0xBAC2DE

VOID KPrint(const CHAR16 *msg, UINT32 x, UINT32 BaseLine, UINT32 color, ...);
