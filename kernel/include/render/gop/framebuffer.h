// kernel/include/render/gop/framebuffer.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

VOID   GOPFrameBufferInit(LINEOS_BOOT_INFO *BootInfo);
VOID   GOPDrawPixel(UINT32 x, UINT32 y, UINT32 color);
UINT32 GOPReadPixel(UINT32 x, UINT32 y);
VOID   GOPFillScreen(UINT32 color);
VOID   GOPFillRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 color);
VOID   GOPDrawRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 color);
VOID   GOPDrawLine(INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT32 color);
VOID   GOPBlendPixel(UINT32 x, UINT32 y, UINT32 color, UINT8 alpha);
VOID   GOPCopyRect(UINT32 SourceX, UINT32 SourceY, UINT32 width, UINT32 height, UINT32 TargetX, UINT32 TargetY);
UINT32 GOPFrameBufferGetWidth(VOID);
UINT32 GOPFrameBufferGetHeight(VOID);
VOID   GOPFrameBufferCopyToLinear(UINT32 *Destination, UINT32 Width, UINT32 Height);
