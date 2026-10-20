// Kernel/Include/Network/VirtIONetwork.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <VirtIO/VirtIOPCI.h>
#include <VirtIO/VirtQueue.h>
#include <Arch/AMD64/SpinLock.h>

#define VIRTIO_NETWORK_VENDOR_ID       0x1AF4
#define VIRTIO_NETWORK_DEVICE_ID       0x1041
#define VIRTIO_NETWORK_TRANSITIONAL_ID 0x1000
#define VIRTIO_NETWORK_IRQ_VECTOR      69
#define VIRTIO_NETWORK_QUEUE_SIZE      64
#define VIRTIO_NETWORK_MAX_FRAME_SIZE  1514
#define VIRTIO_NETWORK_BUFFER_SIZE     2048
#define VIRTIO_NETWORK_F_MAC           (1U << 5)

typedef struct PACKED
{
    UINT8  Flags;
    UINT8  GSOType;
    UINT16 HeaderLength;
    UINT16 GSOSize;
    UINT16 ChecksumStart;
    UINT16 ChecksumOffset;
    UINT16 NumBuffers;
} VIRTIO_NETWORK_HEADER;

typedef struct
{
    VIRTIO_PCI_DEVICE Device;
    VIRTQUEUE         RXQueue;
    VIRTQUEUE         TXQueue;
    SPIN_LOCK         QueueLock;
    UINT8            *RXBuffers;
    UINT8            *TXBuffers;
    UINT8             MAC[6];
    UINT8             Received[VIRTIO_NETWORK_QUEUE_SIZE][VIRTIO_NETWORK_MAX_FRAME_SIZE];
    UINT32            ReceivedLength[VIRTIO_NETWORK_QUEUE_SIZE];
    UINT16            ReceiveHead;
    UINT16            ReceiveTail;
    UINT16            ReceiveCount;
    UINT64            RXPackets;
    UINT64            TXPackets;
    UINT64            RXDropped;
    UINT64            InterruptCount;
    BOOLEAN           Attempted;
    BOOLEAN           Initialized;
} VIRTIO_NETWORK_DEVICE;

BOOLEAN                VirtIONetworkInit(VOID);
BOOLEAN                VirtIONetworkSend(VIRTIO_NETWORK_DEVICE *Device, CONST VOID *Frame, UINT32 Length);
BOOLEAN                VirtIONetworkReceive(VIRTIO_NETWORK_DEVICE *Device, VOID *Frame, UINT32 Capacity, UINT32 *Length);
VOID                   VirtIONetworkPoll(VIRTIO_NETWORK_DEVICE *Device);
VOID                   VirtIONetworkHandleInterrupt(VIRTIO_NETWORK_DEVICE *Device);
VIRTIO_NETWORK_DEVICE *VirtIONetworkGetDevice(VOID);
CONST CHAR16          *VirtIONetworkGetLastError(VOID);
