// Kernel/Source/Disk/VirtIOBlock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Disk/VirtIOBlock.h>
#include <Memory/PMM.h>
#include <PCI/PCI.h>

#define VIRTIO_BLOCK_MAX_REQUESTS      32
#define VIRTIO_BLOCK_DMA_LIMIT         0x100000000ULL
#define VIRTIO_BLOCK_PAGE_SIZE         4096ULL
#define VIRTIO_BLOCK_FEATURE_VERSION_1 (1U << 0)

STATIC VIRTIO_BLOCK_DEVICE VirtIOBlockInfo;
STATIC CONST CHAR16       *LastError = L"not initialized";

STATIC VOID MemoryBarrier(VOID)
{
    ASM("" :: : "memory");
}

STATIC BOOLEAN IsDMABufferValid(CONST VOID *Buffer, UINT64 Length)
{
    UINT64 Address;

    if (Buffer == NULL || Length == 0)
    {
        return FALSE;
    }

    Address = (UINT64) Buffer;

    if (Address >= VIRTIO_BLOCK_DMA_LIMIT || Length > VIRTIO_BLOCK_DMA_LIMIT - Address)
    {
        return FALSE;
    }

    return TRUE;
}

STATIC PCI_DEVICE *FindVirtIOBlockPCIDevice(VOID)
{
    PCI_DEVICE *Device;

    for (UINT32 Index = 0; Index < PCIGetDeviceCount(); Index++)
    {
        Device = PCIGetDevice(Index);

        if (Device == NULL || Device->VendorId != VIRTIO_BLOCK_VENDOR_ID)
        {
            continue;
        }

        if (Device->DeviceId == VIRTIO_BLOCK_DEVICE_ID || Device->DeviceId == VIRTIO_BLOCK_TRANSITIONAL_ID)
        {
            return Device;
        }
    }

    return NULL;
}

STATIC BOOLEAN ReadCapacity(VIRTIO_BLOCK_DEVICE *Device)
{
    UINT8  GenerationBefore;
    UINT8  GenerationAfter;
    UINT64 Capacity;

    if (Device == NULL || Device->Device.DeviceConfig == NULL || Device->Device.CommonConfig == NULL)
    {
        LastError = L"block device config missing";

        return FALSE;
    }

    for (UINT32 Attempt = 0; Attempt < 16; Attempt++)
    {
        GenerationBefore = Device->Device.CommonConfig->ConfigGeneration;
        MemoryBarrier();

        Capacity = 0;

        for (UINT32 Index = 0; Index < sizeof(UINT64); Index++)
        {
            Capacity |= (UINT64) Device->Device.DeviceConfig[Index] << (Index * 8);
        }

        MemoryBarrier();
        GenerationAfter = Device->Device.CommonConfig->ConfigGeneration;

        if (GenerationBefore == GenerationAfter)
        {
            Device->Capacity = Capacity;

            if (Capacity == 0)
            {
                LastError = L"block device capacity is zero";

                return FALSE;
            }

            return TRUE;
        }
    }

    LastError = L"block capacity changed repeatedly";

    return FALSE;
}

STATIC BOOLEAN AllocateRequestPool(VIRTIO_BLOCK_DEVICE *Device)
{
    UINTN RequestBytes;

    RequestBytes = sizeof(VIRTIO_BLOCK_REQUEST) * VIRTIO_BLOCK_MAX_REQUESTS;

    if (RequestBytes > VIRTIO_BLOCK_PAGE_SIZE)
    {
        LastError = L"block request pool too large";

        return FALSE;
    }

    Device->Requests = (VIRTIO_BLOCK_REQUEST *) KAllocPagesBelow(1, VIRTIO_BLOCK_DMA_LIMIT);
    Device->RequestByHead = (VIRTIO_BLOCK_REQUEST **) KAlloc(sizeof(VIRTIO_BLOCK_REQUEST *) * Device->QueueSize);

    if (Device->Requests == NULL || Device->RequestByHead == NULL)
    {
        LastError = L"block request pool allocation failed";

        return FALSE;
    }

    KMemSet(Device->Requests, 0, VIRTIO_BLOCK_PAGE_SIZE);
    KMemSet(Device->RequestByHead, 0, sizeof(VIRTIO_BLOCK_REQUEST *) * Device->QueueSize);

    return TRUE;
}

STATIC VIRTIO_BLOCK_REQUEST *AllocateRequest(VIRTIO_BLOCK_DEVICE *Device)
{
    for (UINT32 Index = 0; Index < VIRTIO_BLOCK_MAX_REQUESTS; Index++)
    {
        if (Device->Requests[Index].State == VIRTIO_BLOCK_REQUEST_FREE)
        {
            VIRTIO_BLOCK_REQUEST *Request = &Device->Requests[Index];

            KMemSet(Request, 0, sizeof(VIRTIO_BLOCK_REQUEST));
            Request->Status = 0xFF;
            Request->State = VIRTIO_BLOCK_REQUEST_PENDING;

            return Request;
        }
    }

    LastError = L"block request pool exhausted";

    return NULL;
}

STATIC BOOLEAN SubmitRequest(VIRTIO_BLOCK_DEVICE *Device, UINT32 Type, UINT64 Sector, UINT32 SectorCount, CONST VOID *Buffer, VIRTIO_BLOCK_REQUEST **OutRequest)
{
    VIRTIO_BLOCK_REQUEST *Request;
    VIRTQUEUE_BUFFER      Buffers[3];
    UINT16                BufferCount;
    UINT16                Head;
    UINT64                DataLength;
    UINT64                Flags;

    if (OutRequest != NULL)
    {
        *OutRequest = NULL;
    }

    if (Device == NULL || !Device->Initialized || OutRequest == NULL)
    {
        LastError = L"block device unavailable";

        return FALSE;
    }

    if (Type != VIRTIO_BLOCK_T_FLUSH)
    {
        if (SectorCount == 0 || SectorCount > UINT32_MAX / VIRTIO_BLOCK_SECTOR_SIZE)
        {
            LastError = L"block sector count invalid";

            return FALSE;
        }

        DataLength = (UINT64) SectorCount * VIRTIO_BLOCK_SECTOR_SIZE;

        if (!IsDMABufferValid(Buffer, DataLength))
        {
            LastError = L"block DMA buffer invalid";

            return FALSE;
        }

        if (Sector >= Device->Capacity || SectorCount > Device->Capacity - Sector)
        {
            LastError = L"block request out of range";

            return FALSE;
        }
    }
    else
    {
        if (!Device->FlushSupported)
        {
            LastError = L"block flush unsupported";

            return FALSE;
        }

        DataLength = 0;
    }

    Flags = SpinLockAcquireIRQSave(&Device->QueueLock);

    Request = AllocateRequest(Device);

    if (Request == NULL)
    {
        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

        return FALSE;
    }

    Request->Header.Type = Type;
    Request->Header.Reserved = 0;
    Request->Header.Sector = Sector;
    Request->Buffer = Buffer;
    Request->Sector = Sector;
    Request->SectorCount = SectorCount;

    Buffers[0].Buffer = &Request->Header;
    Buffers[0].Length = sizeof(VIRTIO_BLOCK_REQUEST_HEADER);
    Buffers[0].Flags = 0;

    BufferCount = 1;

    if (Type != VIRTIO_BLOCK_T_FLUSH)
    {
        Buffers[1].Buffer = (VOID *) Buffer;
        Buffers[1].Length = (UINT32) DataLength;
        Buffers[1].Flags = Type == VIRTIO_BLOCK_T_IN ? VIRTQ_DESC_F_WRITE : 0;

        BufferCount = 2;
    }

    Buffers[BufferCount].Buffer = &Request->Status;
    Buffers[BufferCount].Length = sizeof(Request->Status);
    Buffers[BufferCount].Flags = VIRTQ_DESC_F_WRITE;
    BufferCount++;

    if (!VirtQueueBuildChain(&Device->Queue, Buffers, BufferCount, &Head))
    {
        LastError = VirtQueueGetLastError();
        Request->State = VIRTIO_BLOCK_REQUEST_FREE;
        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

        return FALSE;
    }

    Request->DescriptorHead = Head;
    Device->RequestByHead[Head] = Request;

    MemoryBarrier();

    if (!VirtQueueSubmit(&Device->Device, &Device->Queue, Head))
    {
        LastError = VirtQueueGetLastError();
        Device->RequestByHead[Head] = NULL;
        VirtQueueFreeChain(&Device->Queue, Head);
        Request->State = VIRTIO_BLOCK_REQUEST_FREE;
        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

        return FALSE;
    }

    *OutRequest = Request;
    LastError = L"ok";

    SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

    return TRUE;
}

STATIC BOOLEAN SubmitRequestSync(VIRTIO_BLOCK_DEVICE *Device, UINT32 Type, UINT64 Sector, UINT32 SectorCount, CONST VOID *Buffer)
{
    VIRTIO_BLOCK_REQUEST      *Request;
    VIRTIO_BLOCK_REQUEST_STATE State;
    BOOLEAN                    Result;

    if (!SubmitRequest(Device, Type, Sector, SectorCount, Buffer, &Request))
    {
        return FALSE;
    }

    for (;;)
    {
        State = Request->State;

        if (State != VIRTIO_BLOCK_REQUEST_PENDING)
        {
            break;
        }

        VirtIOBlockHandleInterrupt(Device);

        State = Request->State;

        if (State != VIRTIO_BLOCK_REQUEST_PENDING)
        {
            break;
        }

        ASM("pause" :: : "memory");
    }

    Result = State == VIRTIO_BLOCK_REQUEST_COMPLETED;

    if (!VirtIOBlockReleaseRequest(Device, Request))
    {
        return FALSE;
    }

    return Result;
}

BOOLEAN VirtIOBlockInit(VOID)
{
    PCI_DEVICE *PCIDevice;
    UINT32      FeaturesLow;
    UINT32      FeaturesHigh;

    KMemSet(&VirtIOBlockInfo, 0, sizeof(VirtIOBlockInfo));
    SpinLockInit(&VirtIOBlockInfo.QueueLock);

    PCIDevice = FindVirtIOBlockPCIDevice();

    if (PCIDevice == NULL)
    {
        LastError = L"virtio block pci device missing";

        return FALSE;
    }

    if (!VirtIOPCIInitDevice(&VirtIOBlockInfo.Device, PCIDevice))
    {
        LastError = VirtIOPCIGetLastError();

        return FALSE;
    }

    FeaturesLow = VIRTIO_BLOCK_F_FLUSH;
    FeaturesHigh = 0;

    if (!VirtIOPCIStartDeviceWithFeatures(&VirtIOBlockInfo.Device, FeaturesLow, FeaturesHigh))
    {
        LastError = VirtIOPCIGetLastError();

        return FALSE;
    }

    VirtIOBlockInfo.FlushSupported = (VirtIOBlockInfo.Device.NegotiatedFeaturesLow & VIRTIO_BLOCK_F_FLUSH) != 0;

    if (!VirtQueueInit(&VirtIOBlockInfo.Device, &VirtIOBlockInfo.Queue, VIRTIO_BLOCK_QUEUE_INDEX, 64))
    {
        LastError = VirtQueueGetLastError();

        return FALSE;
    }

    VirtIOBlockInfo.QueueSize = VirtIOBlockInfo.Queue.Size;

    if (!AllocateRequestPool(&VirtIOBlockInfo))
    {
        return FALSE;
    }

    if (!ReadCapacity(&VirtIOBlockInfo))
    {
        return FALSE;
    }

    if (!VirtIOPCIEnableQueueMSIX(&VirtIOBlockInfo.Device, VIRTIO_BLOCK_QUEUE_INDEX, VIRTIO_BLOCK_IRQ_VECTOR))
    {
        LastError = VirtIOPCIGetLastError();

        return FALSE;
    }

    VirtIOPCIReadyDevice(&VirtIOBlockInfo.Device);

    VirtIOBlockInfo.Initialized = TRUE;
    LastError = L"ok";

    return TRUE;
}

BOOLEAN VirtIOBlockReadAsync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 SectorCount, VOID *Buffer, VIRTIO_BLOCK_REQUEST **Request)
{
    return SubmitRequest(Device, VIRTIO_BLOCK_T_IN, Sector, SectorCount, Buffer, Request);
}

BOOLEAN VirtIOBlockWriteAsync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 SectorCount, CONST VOID *Buffer, VIRTIO_BLOCK_REQUEST **Request)
{
    return SubmitRequest(Device, VIRTIO_BLOCK_T_OUT, Sector, SectorCount, Buffer, Request);
}

BOOLEAN VirtIOBlockFlushAsync(VIRTIO_BLOCK_DEVICE *Device, VIRTIO_BLOCK_REQUEST **Request)
{
    return SubmitRequest(Device, VIRTIO_BLOCK_T_FLUSH, 0, 0, NULL, Request);
}

BOOLEAN VirtIOBlockReadSync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 SectorCount, VOID *Buffer)
{
    return SubmitRequestSync(Device, VIRTIO_BLOCK_T_IN, Sector, SectorCount, Buffer);
}

BOOLEAN VirtIOBlockWriteSync(VIRTIO_BLOCK_DEVICE *Device, UINT64 Sector, UINT32 SectorCount, CONST VOID *Buffer)
{
    return SubmitRequestSync(Device, VIRTIO_BLOCK_T_OUT, Sector, SectorCount, Buffer);
}

BOOLEAN VirtIOBlockFlushSync(VIRTIO_BLOCK_DEVICE *Device)
{
    return SubmitRequestSync(Device, VIRTIO_BLOCK_T_FLUSH, 0, 0, NULL);
}

VOID VirtIOBlockHandleInterrupt(VIRTIO_BLOCK_DEVICE *Device)
{
    UINT64 Flags;
    UINT8  InterruptStatus;

    if (Device == NULL || !Device->Initialized)
    {
        return;
    }

    InterruptStatus = *Device->Device.ISRStatus;
    (VOID) InterruptStatus;

    Flags = SpinLockAcquireIRQSave(&Device->QueueLock);

    for (;;)
    {
        VIRTIO_BLOCK_REQUEST *Request;
        UINT16                Head;
        UINT32                UsedLength;

        if (!VirtQueuePopUsed(&Device->Queue, &Head, &UsedLength))
        {
            break;
        }

        if (Head >= Device->QueueSize)
        {
            LastError = L"block used head invalid";
            continue;
        }

        Request = Device->RequestByHead[Head];

        if (Request == NULL)
        {
            LastError = L"block completion has no request";
            continue;
        }

        Request->UsedLength = UsedLength;
        Device->RequestByHead[Head] = NULL;

        if (Request->Status == VIRTIO_BLOCK_S_OK)
        {
            Request->State = VIRTIO_BLOCK_REQUEST_COMPLETED;
        }
        else
        {
            Request->State = VIRTIO_BLOCK_REQUEST_FAILED;
            LastError = Request->Status == VIRTIO_BLOCK_S_UNSUPP ? L"block request unsupported" : L"block I/O error";
        }

        MemoryBarrier();
        VirtQueueFreeChain(&Device->Queue, Head);
    }

    SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);
}

BOOLEAN VirtIOBlockWait(VIRTIO_BLOCK_DEVICE *Device, VIRTIO_BLOCK_REQUEST *Request, UINT32 Timeout)
{
    if (Device == NULL || Request == NULL)
    {
        LastError = L"block wait args invalid";

        return FALSE;
    }

    for (UINT32 Index = 0; Index < Timeout; Index++)
    {
        VIRTIO_BLOCK_REQUEST_STATE State = Request->State;

        if (State == VIRTIO_BLOCK_REQUEST_COMPLETED)
        {
            LastError = L"ok";

            return TRUE;
        }

        if (State == VIRTIO_BLOCK_REQUEST_FAILED || State == VIRTIO_BLOCK_REQUEST_FREE)
        {
            return FALSE;
        }

        ASM("pause" :: : "memory");
    }

    LastError = L"block request wait timeout";

    return FALSE;
}

BOOLEAN VirtIOBlockReleaseRequest(VIRTIO_BLOCK_DEVICE *Device, VIRTIO_BLOCK_REQUEST *Request)
{
    UINT64 Flags;
    UINTN  RequestIndex;

    if (Device == NULL || Request == NULL || Device->Requests == NULL)
    {
        LastError = L"block release args invalid";

        return FALSE;
    }

    RequestIndex = (UINTN) (Request - Device->Requests);

    if (RequestIndex >= VIRTIO_BLOCK_MAX_REQUESTS)
    {
        LastError = L"block request pointer invalid";

        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&Device->QueueLock);

    if (Request->State != VIRTIO_BLOCK_REQUEST_COMPLETED && Request->State != VIRTIO_BLOCK_REQUEST_FAILED)
    {
        LastError = L"block request not completed";
        SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

        return FALSE;
    }

    Request->Buffer = NULL;
    Request->State = VIRTIO_BLOCK_REQUEST_FREE;

    SpinLockReleaseIRQRestore(&Device->QueueLock, Flags);

    LastError = L"ok";

    return TRUE;
}

UINT64 VirtIOBlockGetCapacity(VIRTIO_BLOCK_DEVICE *Device)
{
    return Device != NULL && Device->Initialized ? Device->Capacity : 0;
}

VIRTIO_BLOCK_DEVICE *VirtIOBlockGetDevice(VOID)
{
    return VirtIOBlockInfo.Initialized ? &VirtIOBlockInfo : NULL;
}

CONST CHAR16 *VirtIOBlockGetLastError(VOID)
{
    return LastError;
}
