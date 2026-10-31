// Kernel/Source/Network/VirtIONetwork.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/VirtIONetwork.h>
#include <Memory/PMM.h>
#include <PCI/PCI.h>

#define VIRTIO_NETWORK_DMA_LIMIT 0x100000000ULL
#define VIRTIO_NETWORK_PAGE_SIZE 4096ULL

_Static_assert(sizeof(VIRTIO_NETWORK_HEADER) == 12, "virtio network header size");

STATIC VIRTIO_NETWORK_DEVICE VirtIONetworkInfo;
STATIC CONST CHAR16         *LastError = L"not initialized";

STATIC PCI_DEVICE *FindNetworkDevice(VOID)
{
    for (UINT32 Index = 0; Index < PCIGetDeviceCount(); Index++)
    {
        PCI_DEVICE *Device = PCIGetDevice(Index);

        if (Device != NULL && Device->VendorId == VIRTIO_NETWORK_VENDOR_ID && (Device->DeviceId == VIRTIO_NETWORK_DEVICE_ID || Device->DeviceId == VIRTIO_NETWORK_TRANSITIONAL_ID))
        {
            return Device;
        }
    }

    return NULL;
}

STATIC BOOLEAN FailDevice(CONST CHAR16 *Error)
{
    LastError = Error;
    VirtIONetworkInfo.Initialized = FALSE;

    if (VirtIONetworkInfo.Device.CommonConfig != NULL)
    {
        VirtIONetworkInfo.Device.CommonConfig->DeviceStatus |= VIRTIO_STATUS_FAILED;
    }

    return FALSE;
}

STATIC BOOLEAN ReadMAC(VIRTIO_NETWORK_DEVICE *Device)
{
    for (UINT32 Attempt = 0; Attempt < 16; Attempt++)
    {
        UINT8 Generation = Device->Device.CommonConfig->ConfigGeneration;
        UINT8 Combined = 0;

        ASM("" :: : "memory");

        for (UINT32 Index = 0; Index < 6; Index++)
        {
            Device->MAC[Index] = Device->Device.DeviceConfig[Index];
            Combined |= Device->MAC[Index];
        }

        ASM("" :: : "memory");

        if (Generation == Device->Device.CommonConfig->ConfigGeneration)
        {
            return Combined != 0 && (Device->MAC[0] & 1) == 0;
        }
    }

    return FALSE;
}

STATIC VOID DrainQueues(VIRTIO_NETWORK_DEVICE *Device)
{
    UINT16 Head;
    UINT32 Length;

    while (VirtQueuePopUsed(&Device->TXQueue, &Head, &Length))
    {
        if (Device->TXQueue.DescInUse[Head] == 0)
        {
            FailDevice(L"network TX completion invalid");

            return;
        }

        VirtQueueFreeDesc(&Device->TXQueue, Head);
        Device->TXPackets++;
    }

    while (VirtQueuePopUsed(&Device->RXQueue, &Head, &Length))
    {
        UINT8                 *Buffer = Device->RXBuffers + (UINTN) Head * VIRTIO_NETWORK_BUFFER_SIZE;
        VIRTIO_NETWORK_HEADER *Header = (VIRTIO_NETWORK_HEADER *) Buffer;

        if (Device->RXQueue.DescInUse[Head] == 0)
        {
            FailDevice(L"network RX completion invalid");

            return;
        }

        if (Length >= sizeof(*Header) + 14 && Length <= sizeof(*Header) + VIRTIO_NETWORK_MAX_FRAME_SIZE && Header->Flags == 0 && Header->GSOType == 0 && Device->ReceiveCount < VIRTIO_NETWORK_QUEUE_SIZE)
        {
            UINT32 FrameLength = Length - sizeof(*Header);

            KMemCpy(Device->Received[Device->ReceiveTail], Buffer + sizeof(*Header), FrameLength);
            Device->ReceivedLength[Device->ReceiveTail] = FrameLength;
            Device->ReceiveTail = (Device->ReceiveTail + 1) % VIRTIO_NETWORK_QUEUE_SIZE;
            Device->ReceiveCount++;
            Device->RXPackets++;
        }
        else
        {
            Device->RXDropped++;
        }

        if (!VirtQueueSubmit(&Device->Device, &Device->RXQueue, Head))
        {
            FailDevice(VirtQueueGetLastError());

            return;
        }
    }
}

BOOLEAN VirtIONetworkInit(VOID)
{
    VIRTIO_NETWORK_DEVICE *Device = &VirtIONetworkInfo;
    PCI_DEVICE            *PCIDevice;
    UINTN                  Pages;

    if (Device->Attempted)
    {
        return Device->Initialized;
    }

    Device->Attempted = TRUE;
    SpinLockInit(&Device->QueueLock);
    PCIDevice = FindNetworkDevice();

    if (PCIDevice == NULL)
    {
        return FailDevice(L"virtio network pci device missing");
    }

    if (!VirtIOPCIInitDevice(&Device->Device, PCIDevice) || !VirtIOPCIStartDeviceWithFeatures(&Device->Device, VIRTIO_NETWORK_F_MAC, 0))
    {
        return FailDevice(VirtIOPCIGetLastError());
    }

    if ((Device->Device.NegotiatedFeaturesLow & VIRTIO_NETWORK_F_MAC) == 0 || !ReadMAC(Device))
    {
        return FailDevice(L"network MAC missing or invalid");
    }

    if (!VirtQueueInit(&Device->Device, &Device->RXQueue, 0, VIRTIO_NETWORK_QUEUE_SIZE) || !VirtQueueInit(&Device->Device, &Device->TXQueue, 1, VIRTIO_NETWORK_QUEUE_SIZE))
    {
        return FailDevice(VirtQueueGetLastError());
    }

    Pages = (VIRTIO_NETWORK_QUEUE_SIZE * VIRTIO_NETWORK_BUFFER_SIZE) / VIRTIO_NETWORK_PAGE_SIZE;
    Device->RXBuffers = KAllocPagesBelow(Pages, VIRTIO_NETWORK_DMA_LIMIT);
    Device->TXBuffers = KAllocPagesBelow(Pages, VIRTIO_NETWORK_DMA_LIMIT);

    if (Device->RXBuffers == NULL || Device->TXBuffers == NULL)
    {
        return FailDevice(L"network DMA allocation failed");
    }

    KMemSet(Device->RXBuffers, 0, Pages * VIRTIO_NETWORK_PAGE_SIZE);
    KMemSet(Device->TXBuffers, 0, Pages * VIRTIO_NETWORK_PAGE_SIZE);

    for (UINT16 Index = 0; Index < Device->RXQueue.Size; Index++)
    {
        UINT16 Head;

        if (!VirtQueueAllocDesc(&Device->RXQueue, &Head))
        {
            return FailDevice(VirtQueueGetLastError());
        }

        Device->RXQueue.Desc[Head].Address = (UINT64) (Device->RXBuffers + (UINTN) Head * VIRTIO_NETWORK_BUFFER_SIZE);
        Device->RXQueue.Desc[Head].Length = VIRTIO_NETWORK_BUFFER_SIZE;
        Device->RXQueue.Desc[Head].Flags = VIRTQ_DESC_F_WRITE;

        if (!VirtQueueSubmit(&Device->Device, &Device->RXQueue, Head))
        {
            return FailDevice(VirtQueueGetLastError());
        }
    }

    if (!VirtIOPCIEnableQueueMSIX(&Device->Device, 0, VIRTIO_NETWORK_IRQ_VECTOR) || !VirtIOPCIEnableQueueMSIX(&Device->Device, 1, VIRTIO_NETWORK_IRQ_VECTOR))
    {
        return FailDevice(VirtIOPCIGetLastError());
    }

    Device->Initialized = TRUE;
    ASM("" :: : "memory");
    VirtIOPCIReadyDevice(&Device->Device);
    VirtIOPCINotifyQueue(&Device->Device, 0);
    LastError = L"ok";

    return TRUE;
}

BOOLEAN VirtIONetworkSend(VIRTIO_NETWORK_DEVICE *Device, CONST VOID *Frame, UINT32 Length)
{
    UINT64 Flags;
    UINT16 Head;
    UINT8 *Buffer;

    if (Device == NULL || !Device->Initialized || Frame == NULL || Length < 14 || Length > VIRTIO_NETWORK_MAX_FRAME_SIZE)
    {
        LastError = L"network send args invalid";

        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&Device->QueueLock);
    DrainQueues(Device);

    if (!Device->Initialized || !VirtQueueAllocDesc(&Device->TXQueue, &Head))
    {
        if (Device->Initialized)
        {
            LastError = VirtQueueGetLastError();
        }

        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

        return FALSE;
    }

    Buffer = Device->TXBuffers + (UINTN) Head * VIRTIO_NETWORK_BUFFER_SIZE;
    KMemSet(Buffer, 0, sizeof(VIRTIO_NETWORK_HEADER) + 60);
    KMemCpy(Buffer + sizeof(VIRTIO_NETWORK_HEADER), Frame, Length);
    Device->TXQueue.Desc[Head].Address = (UINT64) Buffer;
    Device->TXQueue.Desc[Head].Length = sizeof(VIRTIO_NETWORK_HEADER) + (Length < 60 ? 60 : Length);

    if (!VirtQueueSubmit(&Device->Device, &Device->TXQueue, Head))
    {
        LastError = VirtQueueGetLastError();
        VirtQueueFreeDesc(&Device->TXQueue, Head);
        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

        return FALSE;
    }

    LastError = L"ok";
    SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

    return TRUE;
}

BOOLEAN VirtIONetworkReceive(VIRTIO_NETWORK_DEVICE *Device, VOID *Frame, UINT32 Capacity, UINT32 *Length)
{
    UINT64  Flags;
    BOOLEAN Result = FALSE;

    if (Length != NULL)
    {
        *Length = 0;
    }

    if (Device == NULL || !Device->Initialized || Frame == NULL || Length == NULL)
    {
        LastError = L"network receive args invalid";

        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&Device->QueueLock);
    DrainQueues(Device);

    if (Device->Initialized && Device->ReceiveCount != 0)
    {
        *Length = Device->ReceivedLength[Device->ReceiveHead];

        if (Capacity >= *Length)
        {
            KMemCpy(Frame, Device->Received[Device->ReceiveHead], *Length);
            Device->ReceiveHead = (Device->ReceiveHead + 1) % VIRTIO_NETWORK_QUEUE_SIZE;
            Device->ReceiveCount--;
            LastError = L"ok";
            Result = TRUE;
        }
        else
        {
            LastError = L"network receive buffer too small";
        }
    }
    else if (Device->Initialized)
    {
        LastError = L"network receive empty";
    }

    SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

    return Result;
}

VOID VirtIONetworkPoll(VIRTIO_NETWORK_DEVICE *Device)
{
    if (Device != NULL && Device->Initialized)
    {
        UINT64 Flags = SpinLockAcquireIRQSave(&Device->QueueLock);

        DrainQueues(Device);
        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);
    }
}

VOID VirtIONetworkHandleInterrupt(VIRTIO_NETWORK_DEVICE *Device)
{
    if (Device != NULL && Device->Initialized)
    {
        UINT64 Flags = SpinLockAcquireIRQSave(&Device->QueueLock);
        UINT8  Status = *Device->Device.ISRStatus;

        (VOID) Status;
        Device->InterruptCount++;
        DrainQueues(Device);
        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);
    }
}

VIRTIO_NETWORK_DEVICE *VirtIONetworkGetDevice(VOID)
{
    return VirtIONetworkInfo.Initialized ? &VirtIONetworkInfo : NULL;
}

CONST CHAR16 *VirtIONetworkGetLastError(VOID)
{
    return LastError;
}
