// virtio_gpu.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu_protocol.h>
#include <virtio/virtio_pci.h>
#include <virtio/virtqueue.h>

#define VIRTIO_GPU_VENDOR_ID 0x1AF4
#define VIRTIO_GPU_DEVICE_ID 0x1050

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
    VIRTIO_GPU_DEBUG                     Debug;
} VIRTIO_GPU_INFO;

BOOLEAN          VirtIOGPUInit(VOID);
BOOLEAN          VirtIOGPUCreateFrameBuffer(UINT32 Width, UINT32 Height);
VOID             FillScreen(UINT32 Color);
VOID             FillRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color);
UINT32           ReadPixel(UINT32 X, UINT32 Y);
BOOLEAN          VirtIOGPUFlush(VOID);
VIRTIO_GPU_INFO *VirtIOGPUGetInfo(VOID);
CONST CHAR16    *VirtIOGPUGetLastError(VOID);
