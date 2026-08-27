// virtqueue.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>
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
} VIRTQUEUE;

BOOLEAN       VirtQueueInit(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, UINT16 QueueIndex, UINT16 WantedSize);
BOOLEAN       VirtQueueSend(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 ResponseLength);
CONST CHAR16 *VirtQueueGetLastError(VOID);
