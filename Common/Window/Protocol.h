// Common/Window/Protocol.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#define LOS_WINDOW_PANEL_HEIGHT 48U
#define LOS_WINDOW_TITLE_HEIGHT 44U
#define LOS_WINDOW_DOCK_HEIGHT  64U

#define LOS_WINDOW_VERSION    1U
#define LOS_WINDOW_CREATE     1U
#define LOS_WINDOW_CREATED    2U
#define LOS_WINDOW_PIXELS     3U
#define LOS_WINDOW_PRESENT    4U
#define LOS_WINDOW_KEY        5U
#define LOS_WINDOW_POINTER    6U
#define LOS_WINDOW_CLOSE      7U
#define LOS_WINDOW_DESTROY    8U
#define LOS_WINDOW_PERSISTENT 1U
#define LOS_WINDOW_BACKGROUND 2U

typedef struct
{
    unsigned int Version;
    unsigned int Type;
    unsigned int Width;
    unsigned int Height;
    unsigned int X;
    unsigned int Y;
    unsigned int Buttons;
    int          Wheel;
    unsigned int Key;
    unsigned int Flags;
    char         Title[64];
} LOS_WINDOW_MESSAGE;
