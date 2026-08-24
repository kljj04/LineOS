// virtqueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <memory/memory.h>
#include <render/gpu/virtio_pci.h>
#include <render/gpu/virtqueue.h>

#define PAGE_SIZE 4096ULL

STATIC CONST CHAR16 *LastError = L"not initialized";

STATIC UINTN AlignUp(UINTN Value, UINTN Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

STATIC UINTN PagesForBytes(UINTN Bytes)
{
    return AlignUp(Bytes, PAGE_SIZE) / PAGE_SIZE;
}

STATIC VOID *AllocQueueMemory(UINTN Bytes)
{
    return KAllocPages(PagesForBytes(Bytes));
}

STATIC VOID MemoryBarrier(VOID)
{
    __asm__ volatile("" ::: "memory");
}

BOOLEAN VirtQueueInit(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, UINT16 QueueIndex, UINT16 WantedSize)
{
    VIRTIO_PCI_COMMON_CONFIG *CommonConfig;
    UINT16 QueueSize;
    UINTN DescBytes;
    UINTN AvailBytes;
    UINTN UsedBytes;

    if (Device == NULL || Device->CommonConfig == NULL || Queue == NULL)
    {
        LastError = L"virtqueue args invalid";
        return FALSE;
    }

    CommonConfig = Device->CommonConfig;
    CommonConfig->QueueSelect = QueueIndex;
    QueueSize = CommonConfig->QueueSize;

    if (QueueSize == 0)
    {
        LastError = L"virtqueue missing";
        return FALSE;
    }

    if (WantedSize != 0 && QueueSize > WantedSize)
    {
        QueueSize = WantedSize;
    }

    DescBytes = sizeof(VIRTQ_DESC) * QueueSize;
    AvailBytes = sizeof(UINT16) * (3 + QueueSize);
    UsedBytes = sizeof(UINT16) * 2 + sizeof(VIRTQ_USED_ELEMENT) * QueueSize;

    KMemSet(Queue, 0, sizeof(VIRTQUEUE));
    Queue->Index = QueueIndex;
    Queue->Size = QueueSize;
    Queue->AvailIndex = 0;
    Queue->UsedIndex = 0;
    Queue->Desc = (VIRTQ_DESC *) AllocQueueMemory(DescBytes);
    Queue->Avail = (VIRTQ_AVAIL *) AllocQueueMemory(AvailBytes);
    Queue->Used = (VIRTQ_USED *) AllocQueueMemory(UsedBytes);

    if (Queue->Desc == NULL || Queue->Avail == NULL || Queue->Used == NULL)
    {
        LastError = L"virtqueue alloc failed";
        return FALSE;
    }

    CommonConfig->QueueSize = QueueSize;
    CommonConfig->QueueDesc = (UINT64) Queue->Desc;
    CommonConfig->QueueDriver = (UINT64) Queue->Avail;
    CommonConfig->QueueDevice = (UINT64) Queue->Used;
    CommonConfig->QueueEnable = 1;

    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtQueueSend(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, VOID *Response,
                      UINT32 ResponseLength)
{
    UINT32 Timeout;

    if (Device == NULL || Queue == NULL || Request == NULL || RequestLength == 0 || Response == NULL ||
        ResponseLength == 0 || Queue->Size < 2)
    {
        LastError = L"virtqueue send args invalid";
        return FALSE;
    }

    Queue->Desc[0].Address = (UINT64) Request;
    Queue->Desc[0].Length = RequestLength;
    Queue->Desc[0].Flags = VIRTQ_DESC_F_NEXT;
    Queue->Desc[0].Next = 1;

    Queue->Desc[1].Address = (UINT64) Response;
    Queue->Desc[1].Length = ResponseLength;
    Queue->Desc[1].Flags = VIRTQ_DESC_F_WRITE;
    Queue->Desc[1].Next = 0;

    Queue->Avail->Ring[Queue->AvailIndex % Queue->Size] = 0;
    MemoryBarrier();
    Queue->AvailIndex++;
    Queue->Avail->Index = Queue->AvailIndex;
    MemoryBarrier();

    VirtIOPCINotifyQueue(Device, Queue->Index);

    for (Timeout = 0; Timeout < 100000000; Timeout++)
    {
        if (Queue->Used->Index != Queue->UsedIndex)
        {
            Queue->UsedIndex = Queue->Used->Index;
            LastError = L"ok";
            return TRUE;
        }
    }

    LastError = L"virtqueue timeout";
    return FALSE;
}

CONST CHAR16 *VirtQueueGetLastError(VOID)
{
    return LastError;
}
