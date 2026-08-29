// virtio_gpu_protocol.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO        0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      0x0101
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_SET_SCANOUT             0x0103
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D     0x0105
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH          0x0104
#define VIRTIO_GPU_RESP_OK_NODATA              0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO        0x1101

#define VIRTIO_GPU_MAX_SCANOUTS          16
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 2

typedef struct PACKED
{
    UINT32 Type;
    UINT32 Flags;
    UINT64 FenceId;
    UINT32 ContextId;
    UINT32 Padding;
} VIRTIO_GPU_CTRL_HEADER;

typedef struct PACKED
{
    UINT32 X;
    UINT32 Y;
    UINT32 Width;
    UINT32 Height;
} VIRTIO_GPU_RECT;

typedef struct PACKED
{
    VIRTIO_GPU_RECT Rect;
    UINT32          Enabled;
    UINT32          Flags;
} VIRTIO_GPU_DISPLAY_ONE;

typedef struct PACKED
{
    VIRTIO_GPU_CTRL_HEADER Header;
} VIRTIO_GPU_GET_DISPLAY_INFO_REQUEST;

typedef struct PACKED
{
    VIRTIO_GPU_CTRL_HEADER Header;
    VIRTIO_GPU_DISPLAY_ONE Displays[VIRTIO_GPU_MAX_SCANOUTS];
} VIRTIO_GPU_GET_DISPLAY_INFO_RESPONSE;

typedef struct PACKED
{
    VIRTIO_GPU_CTRL_HEADER Header;
    UINT32                 ResourceId;
    UINT32                 Format;
    UINT32                 Width;
    UINT32                 Height;
} VIRTIO_GPU_RESOURCE_CREATE_2D_REQUEST;

typedef struct PACKED
{
    UINT64 Address;
    UINT32 Length;
    UINT32 Padding;
} VIRTIO_GPU_MEM_ENTRY;

typedef struct PACKED
{
    VIRTIO_GPU_CTRL_HEADER Header;
    UINT32                 ResourceId;
    UINT32                 EntryCount;
    VIRTIO_GPU_MEM_ENTRY   Entry;
} VIRTIO_GPU_RESOURCE_ATTACH_BACKING_REQUEST;

typedef struct PACKED
{
    VIRTIO_GPU_CTRL_HEADER Header;
    VIRTIO_GPU_RECT        Rect;
    UINT32                 ScanoutId;
    UINT32                 ResourceId;
} VIRTIO_GPU_SET_SCANOUT_REQUEST;

typedef struct PACKED
{
    VIRTIO_GPU_CTRL_HEADER Header;
    VIRTIO_GPU_RECT        Rect;
    UINT64                 Offset;
    UINT32                 ResourceId;
    UINT32                 Padding;
} VIRTIO_GPU_TRANSFER_TO_HOST_2D_REQUEST;

typedef struct PACKED
{
    VIRTIO_GPU_CTRL_HEADER Header;
    VIRTIO_GPU_RECT        Rect;
    UINT32                 ResourceId;
    UINT32                 Padding;
} VIRTIO_GPU_RESOURCE_FLUSH_REQUEST;
