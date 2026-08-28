// font.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>
#include <render/fontinfo.h>

INT32  GOPFindGlyph(UINT16 Unicode);
UINT16 GOPDrawGlyph(UINT16 Unicode, UINT32 x, UINT32 BaseLine, UINT32 Color);