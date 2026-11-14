// Kernel/Include/Render/GPU/VirtIOGPU.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <PCI/PCI.h>
#include <Render/Direct/Direct.h>
#include <Render/GPU/VirtIOGPUProtocol.h>
#include <VirtIO/VirtIOPCI.h>
#include <VirtIO/VirtQueue.h>

#define VIRTIO_GPU_VENDOR_ID       0x1AF4
#define VIRTIO_GPU_DEVICE_ID       0x1050
#define VIRTIO_GPU_IRQ_VECTOR      0x47
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
BOOLEAN          VirtIOGPUEnableInterrupt(VOID);
VOID             VirtIOGPUHandleInterrupt(VOID);
UINT64           VirtIOGPUGetInterruptCount(VOID);
BOOLEAN          VirtIOGPUCreateFrameBuffer(UINT32 Width, UINT32 Height);
VOID             VirtIOGPUMarkDirty(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);
VOID             VirtIOGPUMarkDirtyLocked(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);
VOID             VirtIOGPUClearDirty(VOID);
UINT64           VirtIOGPUAcquireRenderLock(VOID);
VOID             VirtIOGPUReleaseRenderLock(UINT64 Flags);
BOOLEAN          VirtIOGPUSetScanout(VOID);
BOOLEAN          VirtIOGPUTransferRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);
BOOLEAN          VirtIOGPUFlushRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);
BOOLEAN          VirtIOGPUPresent(VOID);
BOOLEAN          VirtIOGPUFlush(VOID);
BOOLEAN          VirtIOGPUPanicLocksAvailable(VOID);
BOOLEAN          VirtIOGPUCreateCursor(UINT32 Width, UINT32 Height, UINT32 HotX, UINT32 HotY);
BOOLEAN          VirtIOGPUSetCursorImage(CONST UINT32 *Pixels, UINT32 Width, UINT32 Height, UINT32 HotX, UINT32 HotY, UINT32 X, UINT32 Y);
BOOLEAN          VirtIOGPUUpdateCursor(UINT32 X, UINT32 Y);
BOOLEAN          VirtIOGPUMoveCursor(UINT32 X, UINT32 Y);
BOOLEAN          VirtIOGPUHideCursor(VOID);
VIRTIO_GPU_INFO *VirtIOGPUGetInfo(VOID);
CONST CHAR16    *VirtIOGPUGetLastError(VOID);
UINT32           VirtIOGPUGetFrameBufferWidth(VOID);
UINT32           VirtIOGPUGetFrameBufferHeight(VOID);
UINT32           VirtIOGPUGetCapsetCount(VOID);
BOOLEAN          VirtIOGPUGetCapsetInfo(UINT32 Index, VIRTIO_GPU_GET_CAPSET_INFO_RESPONSE *Response);
BOOLEAN          VirtIOGPUSendVirGLCommand(VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage);
BOOLEAN          VirtIOGPUReadCapset(UINT32 Id, UINT32 Version, VOID *Data, UINT32 Size, UINT32 *BytesRead);

VOID    VirtIOGPUCheckCompletion(BOOLEAN Interrupt);
BOOLEAN VirtIOGPUSubmitVirGL(VOID *Request, UINT32 RequestLength);
