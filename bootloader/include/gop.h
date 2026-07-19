// gop.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Uefi.h>
#include <lineos/bootinfo.h>

#define GOP_PREFERRED_WIDTH 1920
#define GOP_PREFERRED_HEIGHT 1080

BOOLEAN GOPInit(VOID);

extern LINEOS_GOP GOP;
