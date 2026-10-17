// Kernel/Include/Disk/VirtIOBlock.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <VirtIO/VirtIOPCI.h>
#include <VirtIO/VirtQueue.h>
#include <Arch/AMD64/SpinLock.h>

#define VIRTIO_BLOCK_VENDOR_ID       0x1AF4
#define VIRTIO_BLOCK_DEVICE_ID       0x1042
#define VIRTIO_BLOCK_TRANSITIONAL_ID 0x1001
#define VIRTIO_BLOCK_SECTOR_SIZE     512
#define VIRTIO_BLOCK_QUEUE_INDEX     0
#define VIRTIO_BLOCK_IRQ_VECTOR      68
#define VIRTIO_BLOCK_F_FLUSH         (1U << 9)
#define VIRTIO_BLOCK_T_IN            0
#define VIRTIO_BLOCK_T_OUT           1
#define VIRTIO_BLOCK_T_FLUSH         4
#define VIRTIO_BLOCK_S_OK            0
#define VIRTIO_BLOCK_S_IOERR         1
#define VIRTIO_BLOCK_S_UNSUPP        2

typedef enum
{
    VIRTIO_BLOCK_REQUEST_FREE,
    VIRTIO_BLOCK_REQUEST_PENDING,
    VIRTIO_BLOCK_REQUEST_COMPLETED,
    VIRTIO_BLOCK_REQUEST_FAILED
} VIRTIO_BLOCK_REQUEST_STATE;

typedef struct PACKED
{
    UINT32 Type;
    UINT32 Reserved;
    UINT64 Sector;
} VIRTIO_BLOCK_REQUEST_HEADER;

typedef struct
{
    VIRTIO_BLOCK_REQUEST_HEADER         Header;
    CONST VOID                         *Buffer;
    UINT64                              Sector;
    UINT32                              SectorCount;
    UINT32                              UsedLength;
    UINT16                              DescriptorHead;
    VOLATILE UINT8                      Status;
    VOLATILE VIRTIO_BLOCK_REQUEST_STATE State;
} VIRTIO_BLOCK_REQUEST;

typedef struct
{
    VIRTIO_PCI_DEVICE      Device;
    VIRTQUEUE              Queue;
    SPIN_LOCK              QueueLock;
    VIRTIO_BLOCK_REQUEST  *Requests;
    VIRTIO_BLOCK_REQUEST **RequestByHead;
    UINT64                 Capacity;
    UINT16                 QueueSize;
    BOOLEAN                FlushSupported;
    BOOLEAN                Initialized;
} VIRTIO_BLOCK_DEVICE;

BOOLEAN              VirtIOBlockInit(VOID);
BOOLEAN              VirtIOBlockReadAsync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 SectorCount, VOID *Buffer, VIRTIO_BLOCK_REQUEST **Request);
BOOLEAN              VirtIOBlockWriteAsync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 SectorCount, CONST VOID *Buffer, VIRTIO_BLOCK_REQUEST **Request);
BOOLEAN              VirtIOBlockFlushAsync(VIRTIO_BLOCK_DEVICE *Device, VIRTIO_BLOCK_REQUEST **Request);
VOID                 VirtIOBlockHandleInterrupt(VIRTIO_BLOCK_DEVICE *Device);
BOOLEAN              VirtIOBlockWait(VIRTIO_BLOCK_DEVICE *Device, VIRTIO_BLOCK_REQUEST *Request, UINT32 Timeout);
BOOLEAN              VirtIOBlockReleaseRequest(VIRTIO_BLOCK_DEVICE *Device, VIRTIO_BLOCK_REQUEST *Request);
UINT64               VirtIOBlockGetCapacity(VIRTIO_BLOCK_DEVICE *Device);
VIRTIO_BLOCK_DEVICE *VirtIOBlockGetDevice(VOID);
CONST CHAR16        *VirtIOBlockGetLastError(VOID);
