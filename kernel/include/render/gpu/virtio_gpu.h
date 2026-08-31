// kernel/include/render/gpu/virtio_gpu.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu_protocol.h>
#include <virtio/virtio_pci.h>
#include <virtio/virtqueue.h>

#define VIRTIO_GPU_VENDOR_ID       0x1AF4
#define VIRTIO_GPU_DEVICE_ID       0x1050
#define VIRTIO_GPU_MAX_DIRTY_RECTS 128

typedef struct
{
    BOOLEAN       InitOK;
    BOOLEAN       StartOK;
    BOOLEAN       ControlQueueOK;
    BOOLEAN       CursorQueueOK;
    BOOLEAN       DisplayInfoOK;
    BOOLEAN       CreateOK;
    BOOLEAN       AttachOK;
    BOOLEAN       TransferOK;
    BOOLEAN       SetScanoutOK;
    BOOLEAN       FlushOK;
    UINT32        LastCommand;
    UINT32        LastResponse;
    CONST CHAR16 *LastStage;
} VIRTIO_GPU_DEBUG;

typedef struct
{
    BOOLEAN                              Found;
    VIRTIO_PCI_DEVICE                    Device;
    VIRTQUEUE                            ControlQueue;
    VIRTQUEUE                            CursorQueue;
    VIRTIO_GPU_GET_DISPLAY_INFO_RESPONSE DisplayInfo;
    UINT32                              *FrameBuffer;
    UINT32                               FrameBufferWidth;
    UINT32                               FrameBufferHeight;
    UINT32                               ResourceId;
    BOOLEAN                              ScanoutSet;
    BOOLEAN                              Dirty;
    UINT32                               DirtyRectCount;
    VIRTIO_GPU_RECT                      DirtyRects[VIRTIO_GPU_MAX_DIRTY_RECTS];
    UINT32                              *CursorBuffer;
    UINT32                               CursorResourceId;
    UINT32                               CursorWidth;
    UINT32                               CursorHeight;
    UINT32                               CursorHotX;
    UINT32                               CursorHotY;
    VIRTIO_GPU_DEBUG                     Debug;
} VIRTIO_GPU_INFO;

BOOLEAN          VirtIOGPUInit(VOID);
BOOLEAN          VirtIOGPUCreateFrameBuffer(UINT32 Width, UINT32 Height);
VOID             FillScreen(UINT32 Color);
VOID             DrawPixel(UINT32 X, UINT32 Y, UINT32 Color);
VOID             FillRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color);
VOID             DrawRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color);
VOID             DrawLine(INT32 X1, INT32 Y1, INT32 X2, INT32 Y2, UINT32 Color);
VOID             DrawCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color);
VOID             FillCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color);
VOID             VirtIOGPUBlendPixel(UINT32 X, UINT32 Y, UINT32 Color, UINT8 Alpha);
VOID             CopyRect(UINT32 SourceX, UINT32 SourceY, UINT32 Width, UINT32 Height, UINT32 TargetX, UINT32 TargetY);
UINT32           ReadPixel(UINT32 X, UINT32 Y);
VOID             VirtIOGPUMarkDirty(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);
VOID             VirtIOGPUClearDirty(VOID);
BOOLEAN          VirtIOGPUSetScanout(VOID);
BOOLEAN          VirtIOGPUTransferRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);
BOOLEAN          VirtIOGPUFlushRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);
BOOLEAN          VirtIOGPUPresent(VOID);
BOOLEAN          VirtIOGPUFlush(VOID);
BOOLEAN          VirtIOGPUCreateCursor(UINT32 Width, UINT32 Height, UINT32 HotX, UINT32 HotY);
BOOLEAN          VirtIOGPUSetCursorImage(CONST UINT32 *Pixels, UINT32 Width, UINT32 Height, UINT32 HotX, UINT32 HotY, UINT32 X, UINT32 Y);
BOOLEAN          VirtIOGPUUpdateCursor(UINT32 X, UINT32 Y);
BOOLEAN          VirtIOGPUMoveCursor(UINT32 X, UINT32 Y);
VIRTIO_GPU_INFO *VirtIOGPUGetInfo(VOID);
CONST CHAR16    *VirtIOGPUGetLastError(VOID);
UINT32           VirtIOGPUGetFrameBufferWidth(VOID);
UINT32           VirtIOGPUGetFrameBufferHeight(VOID);
