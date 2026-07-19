// framebuffer.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

VOID FrameBufferInit(LINEOS_BOOT_INFO *BootInfo);
VOID DrawPixel(UINT32 x, UINT32 y, UINT32 color);
UINT32 ReadPixel(UINT32 x, UINT32 y);
VOID FillScreen(UINT32 color);
VOID FillRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 color);
VOID DrawRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 color);
VOID DrawLine(INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT32 color);
VOID BlendPixel(UINT32 x, UINT32 y, UINT32 color, UINT8 alpha);
VOID CopyRect(UINT32 SourceX, UINT32 SourceY, UINT32 width, UINT32 height, UINT32 TargetX, UINT32 TargetY);
