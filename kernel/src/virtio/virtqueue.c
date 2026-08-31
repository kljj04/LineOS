// kernel/src/virtio/virtqueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <memory/memory.h>
#include <virtio/virtio_pci.h>
#include <virtio/virtqueue.h>

#define PAGE_SIZE        4096ULL
#define VIRTIO_DMA_LIMIT 0x100000000ULL

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
    return KAllocPagesBelow(PagesForBytes(Bytes), VIRTIO_DMA_LIMIT);
}

STATIC VOID MemoryBarrier(VOID)
{
    ASM("" ::: "memory");
}

STATIC BOOLEAN DescIndexValid(VIRTQUEUE *Queue, UINT16 DescIndex)
{
    return Queue != NULL && DescIndex < Queue->Size;
}

STATIC BOOLEAN ConsumeCompleted(VIRTQUEUE *Queue, UINT16 Head, UINT32 *Length)
{
    if (!DescIndexValid(Queue, Head) || Queue->Completed[Head] == 0)
    {
        return FALSE;
    }

    if (Length != NULL)
    {
        *Length = Queue->CompletedLength[Head];
    }

    Queue->Completed[Head] = 0;
    Queue->CompletedLength[Head] = 0;
    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtQueueInit(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, UINT16 QueueIndex, UINT16 WantedSize)
{
    VIRTIO_PCI_COMMON_CONFIG *CommonConfig;
    UINT16                    QueueSize;
    UINTN                     DescBytes;
    UINTN                     AvailBytes;
    UINTN                     UsedBytes;
    UINTN                     DescInUseBytes;
    UINTN                     CompletedBytes;
    UINTN                     CompletedLengthBytes;

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
    DescInUseBytes = sizeof(UINT8) * QueueSize;
    CompletedBytes = sizeof(UINT8) * QueueSize;
    CompletedLengthBytes = sizeof(UINT32) * QueueSize;

    KMemSet(Queue, 0, sizeof(VIRTQUEUE));
    Queue->Index = QueueIndex;
    Queue->Size = QueueSize;
    Queue->AvailIndex = 0;
    Queue->UsedIndex = 0;
    Queue->FreeHead = 0;
    Queue->FreeCount = QueueSize;
    Queue->Desc = (VIRTQ_DESC *) AllocQueueMemory(DescBytes);
    Queue->Avail = (VIRTQ_AVAIL *) AllocQueueMemory(AvailBytes);
    Queue->Used = (VIRTQ_USED *) AllocQueueMemory(UsedBytes);
    Queue->DescInUse = (UINT8 *) AllocQueueMemory(DescInUseBytes);
    Queue->Completed = (UINT8 *) AllocQueueMemory(CompletedBytes);
    Queue->CompletedLength = (UINT32 *) AllocQueueMemory(CompletedLengthBytes);

    if (Queue->Desc == NULL || Queue->Avail == NULL || Queue->Used == NULL || Queue->DescInUse == NULL || Queue->Completed == NULL || Queue->CompletedLength == NULL)
    {
        LastError = L"virtqueue alloc failed";
        return FALSE;
    }

    KMemSet(Queue->Desc, 0, DescBytes);
    KMemSet(Queue->Avail, 0, AvailBytes);
    KMemSet(Queue->Used, 0, UsedBytes);
    KMemSet(Queue->DescInUse, 0, DescInUseBytes);
    KMemSet(Queue->Completed, 0, CompletedBytes);
    KMemSet(Queue->CompletedLength, 0, CompletedLengthBytes);

    for (UINT16 Index = 0; Index < QueueSize; Index++)
    {
        Queue->Desc[Index].Next = Index + 1;
    }
    Queue->Desc[QueueSize - 1].Next = 0xFFFF;

    CommonConfig->QueueSize = QueueSize;
    CommonConfig->QueueDesc = (UINT64) Queue->Desc;
    CommonConfig->QueueDriver = (UINT64) Queue->Avail;
    CommonConfig->QueueDevice = (UINT64) Queue->Used;
    CommonConfig->QueueEnable = 1;

    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtQueueAllocDesc(VIRTQUEUE *Queue, UINT16 *DescIndex)
{
    UINT16 Index;

    if (Queue == NULL || DescIndex == NULL)
    {
        LastError = L"virtqueue alloc args invalid";
        return FALSE;
    }

    if (Queue->FreeCount == 0 || Queue->FreeHead == 0xFFFF)
    {
        LastError = L"virtqueue descriptors exhausted";
        return FALSE;
    }

    Index = Queue->FreeHead;
    Queue->FreeHead = Queue->Desc[Index].Next;
    Queue->FreeCount--;
    Queue->DescInUse[Index] = 1;
    Queue->Completed[Index] = 0;
    Queue->CompletedLength[Index] = 0;
    KMemSet(&Queue->Desc[Index], 0, sizeof(VIRTQ_DESC));

    *DescIndex = Index;
    LastError = L"ok";
    return TRUE;
}

VOID VirtQueueFreeDesc(VIRTQUEUE *Queue, UINT16 DescIndex)
{
    if (!DescIndexValid(Queue, DescIndex) || Queue->DescInUse[DescIndex] == 0)
    {
        LastError = L"virtqueue free args invalid";
        return;
    }

    KMemSet(&Queue->Desc[DescIndex], 0, sizeof(VIRTQ_DESC));
    Queue->Desc[DescIndex].Next = Queue->FreeHead;
    Queue->DescInUse[DescIndex] = 0;
    Queue->Completed[DescIndex] = 0;
    Queue->CompletedLength[DescIndex] = 0;
    Queue->FreeHead = DescIndex;
    Queue->FreeCount++;
    LastError = L"ok";
}

VOID VirtQueueFreeChain(VIRTQUEUE *Queue, UINT16 Head)
{
    UINT16 Index;

    if (!DescIndexValid(Queue, Head))
    {
        LastError = L"virtqueue chain free args invalid";
        return;
    }

    Index = Head;
    while (DescIndexValid(Queue, Index) && Queue->DescInUse[Index] != 0)
    {
        UINT16 Flags = Queue->Desc[Index].Flags;
        UINT16 Next = Queue->Desc[Index].Next;

        VirtQueueFreeDesc(Queue, Index);

        if ((Flags & VIRTQ_DESC_F_NEXT) == 0)
        {
            break;
        }

        Index = Next;
    }

    LastError = L"ok";
}

BOOLEAN VirtQueueBuildChain(VIRTQUEUE *Queue, VIRTQUEUE_BUFFER *Buffers, UINT16 BufferCount, UINT16 *Head)
{
    UINT16 First = 0xFFFF;
    UINT16 Previous = 0xFFFF;

    if (Queue == NULL || Buffers == NULL || BufferCount == 0 || Head == NULL)
    {
        LastError = L"virtqueue chain args invalid";
        return FALSE;
    }

    for (UINT16 Index = 0; Index < BufferCount; Index++)
    {
        UINT16 DescIndex;

        if (Buffers[Index].Buffer == NULL || Buffers[Index].Length == 0)
        {
            if (First != 0xFFFF)
            {
                VirtQueueFreeChain(Queue, First);
            }

            LastError = L"virtqueue buffer invalid";
            return FALSE;
        }

        if (!VirtQueueAllocDesc(Queue, &DescIndex))
        {
            if (First != 0xFFFF)
            {
                VirtQueueFreeChain(Queue, First);
            }

            return FALSE;
        }

        Queue->Desc[DescIndex].Address = (UINT64) Buffers[Index].Buffer;
        Queue->Desc[DescIndex].Length = Buffers[Index].Length;
        Queue->Desc[DescIndex].Flags = Buffers[Index].Flags & VIRTQ_DESC_F_WRITE;
        Queue->Desc[DescIndex].Next = 0;

        if (Previous != 0xFFFF)
        {
            Queue->Desc[Previous].Flags |= VIRTQ_DESC_F_NEXT;
            Queue->Desc[Previous].Next = DescIndex;
        }
        else
        {
            First = DescIndex;
        }

        Previous = DescIndex;
    }

    *Head = First;
    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtQueueSubmit(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, UINT16 Head)
{
    if (Device == NULL || Queue == NULL || Head >= Queue->Size || Queue->DescInUse[Head] == 0)
    {
        LastError = L"virtqueue submit args invalid";
        return FALSE;
    }

    MemoryBarrier();
    Queue->Avail->Ring[Queue->AvailIndex % Queue->Size] = Head;
    MemoryBarrier();
    Queue->AvailIndex++;
    Queue->Avail->Index = Queue->AvailIndex;
    MemoryBarrier();

    VirtIOPCINotifyQueue(Device, Queue->Index);

    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtQueuePostBuffer(VIRTIO_PCI_DEVICE *Device, VIRTQUEUE *Queue, VOID *Buffer, UINT32 Length, UINT16 Flags, UINT16 *Head)
{
    VIRTQUEUE_BUFFER QueueBuffer;
    UINT16           DescHead;

    QueueBuffer.Buffer = Buffer;
    QueueBuffer.Length = Length;
    QueueBuffer.Flags = Flags;

    if (!VirtQueueBuildChain(Queue, &QueueBuffer, 1, &DescHead))
    {
        return FALSE;
    }

    if (!VirtQueueSubmit(Device, Queue, DescHead))
    {
        VirtQueueFreeChain(Queue, DescHead);
        return FALSE;
    }

    if (Head != NULL)
    {
        *Head = DescHead;
    }

    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtQueuePopUsed(VIRTQUEUE *Queue, UINT16 *Head, UINT32 *Length)
{
    VIRTQ_USED_ELEMENT UsedElement;

    if (Queue == NULL || Head == NULL)
    {
        LastError = L"virtqueue pop args invalid";
        return FALSE;
    }

    if (Queue->Used->Index == Queue->UsedIndex)
    {
        LastError = L"virtqueue used empty";
        return FALSE;
    }

    MemoryBarrier();
    UsedElement = Queue->Used->Ring[Queue->UsedIndex % Queue->Size];
    Queue->UsedIndex++;

    if (UsedElement.Id >= Queue->Size)
    {
        LastError = L"virtqueue used id invalid";
        return FALSE;
    }

    *Head = (UINT16) UsedElement.Id;
    if (Length != NULL)
    {
        *Length = UsedElement.Length;
    }

    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtQueueWaitUsed(VIRTQUEUE *Queue, UINT16 Head, UINT32 *Length, UINT32 Timeout)
{
    if (!DescIndexValid(Queue, Head))
    {
        LastError = L"virtqueue wait args invalid";
        return FALSE;
    }

    if (ConsumeCompleted(Queue, Head, Length))
    {
        return TRUE;
    }

    for (UINT32 Index = 0; Index < Timeout; Index++)
    {
        UINT16 UsedHead;
        UINT32 UsedLength;

        if (!VirtQueuePopUsed(Queue, &UsedHead, &UsedLength))
        {
            continue;
        }

        if (UsedHead == Head)
        {
            if (Length != NULL)
            {
                *Length = UsedLength;
            }

            LastError = L"ok";
            return TRUE;
        }

        Queue->Completed[UsedHead] = 1;
        Queue->CompletedLength[UsedHead] = UsedLength;
    }

    LastError = L"virtqueue timeout";
    return FALSE;
}

CONST CHAR16 *VirtQueueGetLastError(VOID)
{
    return LastError;
}
