// kernel/include/virtio/virtqueue.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <virtio/virtio_pci.h>

#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2

typedef struct PACKED
{
    UINT64 Address;
    UINT32 Length;
    UINT16 Flags;
    UINT16 Next;
} VIRTQ_DESC;

typedef struct PACKED
{
    UINT16 Flags;
    UINT16 Index;
    UINT16 Ring[];
} VIRTQ_AVAIL;

typedef struct PACKED
{
    UINT32 Id;
    UINT32 Length;
} VIRTQ_USED_ELEMENT;

typedef struct PACKED
{
    UINT16             Flags;
    UINT16             Index;
    VIRTQ_USED_ELEMENT Ring[];
} VIRTQ_USED;

typedef struct
{
    UINT16       Index;
    UINT16       Size;
    VIRTQ_DESC  *Desc;
    VIRTQ_AVAIL *Avail;
    VIRTQ_USED  *Used;
    UINT16       AvailIndex;
    UINT16       UsedIndex;
    UINT16       FreeHead;
    UINT16       FreeCount;
    UINT8       *DescInUse;
    UINT8       *Completed;
    UINT32      *CompletedLength;
} VIRTQUEUE;

typedef struct
{
    VOID  *Buffer;
    UINT32 Length;
    UINT16 Flags;
} VIRTQUEUE_BUFFER;

BOOLEAN       VirtQueueInit(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, UINT16 QueueIndex, UINT16 WantedSize);
BOOLEAN       VirtQueueAllocDesc(VIRTQUEUE *Queue, UINT16 *DescIndex);
VOID          VirtQueueFreeDesc(VIRTQUEUE *Queue, UINT16 DescIndex);
VOID          VirtQueueFreeChain(VIRTQUEUE *Queue, UINT16 Head);
BOOLEAN       VirtQueueBuildChain(VIRTQUEUE *Queue, VIRTQUEUE_BUFFER *Buffers, UINT16 BufferCount, UINT16 *Head);
BOOLEAN       VirtQueueSubmit(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, UINT16 Head);
BOOLEAN       VirtQueuePostBuffer(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, VOID *Buffer, UINT32 Length, UINT16 Flags, UINT16 *Head);
BOOLEAN       VirtQueuePopUsed(VIRTQUEUE *Queue, UINT16 *Head, UINT32 *Length);
BOOLEAN       VirtQueueWaitUsed(VIRTQUEUE *Queue, UINT16 Head, UINT32 *Length, UINT32 Timeout);
CONST CHAR16 *VirtQueueGetLastError(VOID);
