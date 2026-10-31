// LOSLibC/Include/Window.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <IPC.h>
#include <Window/Protocol.h>

typedef struct
{
    UINT64  Endpoint;
    UINT64  Server;
    UINT32  Width;
    UINT32  Height;
    UINT8  *Transfer;
    UINT32 *Previous;
    BOOLEAN Presented;
} LOS_WINDOW;

INT32 LOSWindowOpen(LOS_WINDOW *Window, UINT64 Server, CONST char *Title, UINT32 Width, UINT32 Height, UINT32 Flags);
INT32 LOSWindowPresent(LOS_WINDOW *Window, CONST UINT32 *Pixels);
INT32 LOSWindowPoll(LOS_WINDOW *Window, LOS_WINDOW_MESSAGE *Event);
VOID  LOSWindowClose(LOS_WINDOW *Window);

INT32 LOSWindowPresentRows(LOS_WINDOW *Window, CONST UINT32 *Pixels, UINT32 First, UINT32 Last);
