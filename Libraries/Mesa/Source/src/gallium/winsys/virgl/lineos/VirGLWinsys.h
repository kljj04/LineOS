// Libraries/Mesa/Source/src/gallium/winsys/virgl/lineos/VirGLWinsys.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include "gallium/drivers/virgl/virgl_winsys.h"

typedef struct
{
    struct virgl_winsys Base;
    int                 FD;
} MESA_LINEOS_VIRGL_WINSYS;

void MesaLineOSInitVirGLCaps(MESA_LINEOS_VIRGL_WINSYS *Winsys, int FD);
