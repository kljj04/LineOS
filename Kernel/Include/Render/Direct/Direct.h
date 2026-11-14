// Kernel/Include/Render/Direct/Direct.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

VOID   FillScreen(UINT32 Color);
VOID   DrawPixel(UINT32 X, UINT32 Y, UINT32 Color);
VOID   FillRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color);
VOID   DrawRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color);
VOID   DrawLine(INT32 X1, INT32 Y1, INT32 X2, INT32 Y2, UINT32 Color);
VOID   DrawCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color);
VOID   FillCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color);
VOID   VirtIOGPUBlendPixel(UINT32 X, UINT32 Y, UINT32 Color, UINT8 Alpha);
VOID   CopyRect(UINT32 SourceX, UINT32 SourceY, UINT32 Width, UINT32 Height, UINT32 TargetX, UINT32 TargetY);
UINT32 ReadPixel(UINT32 X, UINT32 Y);
