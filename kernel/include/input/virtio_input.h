// kernel/include/input/virtio_input.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <input/virtio_input_protocol.h>

BOOLEAN       VirtIOInputInit(VOID);
VOID          VirtIOKeyboardInterruptHandler(VOID);
VOID          VirtIOTabletInterruptHandler(VOID);
BOOLEAN       VirtIOInputIsKeyboardAvailable(VOID);
BOOLEAN       VirtIOInputIsTabletAvailable(VOID);
BOOLEAN       VirtIOInputHasPendingEvent(VOID);
BOOLEAN       VirtIOInputGetKeyEvent(VIRTIO_KEY_EVENT *Event);
BOOLEAN       VirtIOInputGetPointerEvent(VIRTIO_POINTER_EVENT *Event);
CONST CHAR16 *VirtIOInputGetLastError(VOID);
BOOLEAN       VirtIOInputGetTabletRange(UINT32 *MinX, UINT32 *MaxX, UINT32 *MinY, UINT32 *MaxY);
