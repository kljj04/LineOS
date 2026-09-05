// kernel/src/memory/memory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/spinlock.h>
#include <lineos/bootinfo.h>
#include <memory/memory.h>

#define PAGE_SIZE                  4096ULL
#define LOW_MEMORY_RESERVED_PAGES 256ULL
#define BUDDY_MAX_ORDER            20

#define HEAP_ALIGNMENT      16ULL
#define HEAP_INITIAL_PAGES  16ULL
#define HEAP_EXPAND_PAGES   16ULL
#define HEAP_MIN_BLOCK_SIZE 16ULL

STATIC BUDDY_BLOCK *FreeLists[BUDDY_MAX_ORDER + 1];
STATIC UINT64 TotalPages = 0;
STATIC SPIN_LOCK BuddyLock;

STATIC HEAP_BLOCK *HeapHead = NULL;
STATIC HEAP_BLOCK *HeapTail = NULL;
STATIC BOOLEAN HeapInitialized = FALSE;
STATIC SPIN_LOCK HeapLock;

STATIC UINT64 AlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

STATIC UINT64 AlignDown(UINT64 Value, UINT64 Alignment)
{
    return Value & ~(Alignment - 1);
}

STATIC UINTN PTOO(UINTN PageCount)
{
    UINTN order = 0;
    UINTN pages = 1;

    while (pages < PageCount)
    {
        pages <<= 1;
        order++;
    }

    return order;
}

STATIC UINT64 OrderToPages(UINTN order)
{
    return 1ULL << order;
}

STATIC UINT64 OrderToSize(UINTN order)
{
    return PAGE_SIZE << order;
}

STATIC EFI_MEMORY_DESCRIPTOR *GetMemoryDescriptor(LINEOS_MEMORY_MAP *MemoryMap, UINTN index)
{
    return (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *) MemoryMap->MemoryMap + (index * MemoryMap->MemoryMapDescriptorSize));
}

STATIC UINTN GetMemoryDescriptorCount(LINEOS_MEMORY_MAP *MemoryMap)
{
    return MemoryMap->MemoryMapSize / MemoryMap->MemoryMapDescriptorSize;
}

STATIC UINT64 FindHighestConventionalMemoryAddress(LINEOS_MEMORY_MAP *MemoryMap)
{
    UINT64 HighestAddress = 0;
    UINTN EntryCount = GetMemoryDescriptorCount(MemoryMap);

    for (UINTN index = 0; index < EntryCount; index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = GetMemoryDescriptor(MemoryMap, index);
        UINT64 end;

        if (Descriptor->Type != EFI_CONVENTIONAL_MEMORY)
        {
            continue;
        }

        end = Descriptor->PhysicalStart + (Descriptor->NumberOfPages * PAGE_SIZE);

        if (end > HighestAddress)
        {
            HighestAddress = end;
        }
    }

    return HighestAddress;
}

STATIC VOID BuddyPush(UINTN order, UINT64 address)
{
    BUDDY_BLOCK *block;

    if (order > BUDDY_MAX_ORDER)
    {
        return;
    }

    block = (BUDDY_BLOCK *) address;
    block->Next = FreeLists[order];
    FreeLists[order] = block;
}

STATIC BUDDY_BLOCK *BuddyPop(UINTN order)
{
    BUDDY_BLOCK *block;

    if (order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    block = FreeLists[order];

    if (block == NULL)
    {
        return NULL;
    }

    FreeLists[order] = block->Next;
    block->Next = NULL;

    return block;
}

STATIC BOOLEAN BuddyRemove(UINTN order, UINT64 address)
{
    BUDDY_BLOCK *block;
    BUDDY_BLOCK *previous = NULL;

    if (order > BUDDY_MAX_ORDER)
    {
        return FALSE;
    }

    block = FreeLists[order];

    while (block != NULL)
    {
        if ((UINT64) block == address)
        {
            if (previous == NULL)
            {
                FreeLists[order] = block->Next;
            }
            else
            {
                previous->Next = block->Next;
            }

            block->Next = NULL;
            return TRUE;
        }

        previous = block;
        block = block->Next;
    }

    return FALSE;
}

STATIC UINTN LargestOrderForRange(UINT64 address, UINT64 PageCount)
{
    UINTN order = 0;

    while (order < BUDDY_MAX_ORDER)
    {
        UINTN NextOrder = order + 1;
        UINT64 NextPages = OrderToPages(NextOrder);
        UINT64 NextSize = OrderToSize(NextOrder);

        if (NextPages > PageCount)
        {
            break;
        }

        if ((address & (NextSize - 1)) != 0)
        {
            break;
        }

        order = NextOrder;
    }

    return order;
}

STATIC VOID BuddyAddRange(UINT64 address, UINT64 PageCount)
{
    while (PageCount != 0)
    {
        UINTN order = LargestOrderForRange(address, PageCount);
        UINT64 pages = OrderToPages(order);

        BuddyPush(order, address);

        address += pages * PAGE_SIZE;
        PageCount -= pages;
    }
}

STATIC VOID BuddyAddUsableRange(UINT64 address, UINT64 PageCount)
{
    UINT64 start;
    UINT64 end;
    UINT64 ReservedEnd = LOW_MEMORY_RESERVED_PAGES * PAGE_SIZE;

    if (PageCount == 0)
    {
        return;
    }

    start = AlignUp(address, PAGE_SIZE);
    end = AlignDown(address + (PageCount * PAGE_SIZE), PAGE_SIZE);

    if (end <= start)
    {
        return;
    }

    if (end <= ReservedEnd)
    {
        return;
    }

    if (start < ReservedEnd)
    {
        start = ReservedEnd;
    }

    if (end <= start)
    {
        return;
    }

    BuddyAddRange(start, (end - start) / PAGE_SIZE);
}

STATIC VOID BuddyAddUsableRangeExcept(UINT64 address, UINT64 PageCount, UINT64 ReservedStart, UINT64 ReservedEnd)
{
    UINT64 start;
    UINT64 end;

    if (PageCount == 0)
    {
        return;
    }

    start = AlignUp(address, PAGE_SIZE);
    end = AlignDown(address + (PageCount * PAGE_SIZE), PAGE_SIZE);

    if (end <= start)
    {
        return;
    }

    if (ReservedEnd <= ReservedStart || end <= ReservedStart || start >= ReservedEnd)
    {
        BuddyAddUsableRange(start, (end - start) / PAGE_SIZE);
        return;
    }

    if (start < ReservedStart)
    {
        BuddyAddUsableRange(start, (ReservedStart - start) / PAGE_SIZE);
    }

    if (end > ReservedEnd)
    {
        BuddyAddUsableRange(ReservedEnd, (end - ReservedEnd) / PAGE_SIZE);
    }
}

STATIC VOID *BuddyAllocate(UINTN order)
{
    UINTN CurrentOrder;
    BUDDY_BLOCK *block;
    UINT64 address;

    if (order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    CurrentOrder = order;

    while (CurrentOrder <= BUDDY_MAX_ORDER && FreeLists[CurrentOrder] == NULL)
    {
        CurrentOrder++;
    }

    if (CurrentOrder > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    block = BuddyPop(CurrentOrder);

    if (block == NULL)
    {
        return NULL;
    }

    address = (UINT64) block;

    while (CurrentOrder > order)
    {
        UINT64 BuddyAddress;

        CurrentOrder--;
        BuddyAddress = address + OrderToSize(CurrentOrder);

        BuddyPush(CurrentOrder, BuddyAddress);
    }

    return (VOID *) address;
}

STATIC VOID *BuddyAllocateBelow(UINTN order, UINT64 limit)
{
    if (order > BUDDY_MAX_ORDER || limit == 0)
    {
        return NULL;
    }

    for (UINTN CurrentOrder = order; CurrentOrder <= BUDDY_MAX_ORDER; CurrentOrder++)
    {
        BUDDY_BLOCK *block = FreeLists[CurrentOrder];

        while (block != NULL)
        {
            UINT64 address = (UINT64) block;
            UINT64 RequestedSize = OrderToSize(order);

            if (address <= limit && RequestedSize <= limit - address)
            {
                if (!BuddyRemove(CurrentOrder, address))
                {
                    return NULL;
                }

                while (CurrentOrder > order)
                {
                    UINT64 BuddyAddress;

                    CurrentOrder--;
                    BuddyAddress = address + OrderToSize(CurrentOrder);

                    BuddyPush(CurrentOrder, BuddyAddress);
                }

                return (VOID *) address;
            }

            block = block->Next;
        }
    }

    return NULL;
}

STATIC VOID BuddyFree(UINT64 address, UINTN order)
{
    if (order > BUDDY_MAX_ORDER)
    {
        return;
    }

    while (order < BUDDY_MAX_ORDER)
    {
        UINT64 size = OrderToSize(order);
        UINT64 BuddyAddress = address ^ size;

        if (!BuddyRemove(order, BuddyAddress))
        {
            break;
        }

        if (BuddyAddress < address)
        {
            address = BuddyAddress;
        }

        order++;
    }

    BuddyPush(order, address);
}

BOOLEAN KMemoryInit(LINEOS_BOOT_INFO *BootInfo)
{
    LINEOS_MEMORY_MAP *MemoryMap;
    UINTN EntryCount;
    UINT64 HighestAddress;
    UINT64 KernelStart;
    UINT64 KernelEnd;

    if (BootInfo == NULL || BootInfo->MemoryMap == NULL || BootInfo->MemoryMap->MemoryMap == NULL || BootInfo->MemoryMap->MemoryMapDescriptorSize == 0)
    {
        return FALSE;
    }

    SpinLockInit(&BuddyLock);
    SpinLockInit(&HeapLock);

    for (UINTN order = 0; order <= BUDDY_MAX_ORDER; order++)
    {
        FreeLists[order] = NULL;
    }

    MemoryMap = BootInfo->MemoryMap;
    EntryCount = GetMemoryDescriptorCount(MemoryMap);
    HighestAddress = FindHighestConventionalMemoryAddress(MemoryMap);

    if (HighestAddress == 0)
    {
        return FALSE;
    }

    TotalPages = AlignUp(HighestAddress, PAGE_SIZE) / PAGE_SIZE;
    KernelStart = AlignDown(BootInfo->Kernel.Base, PAGE_SIZE);
    KernelEnd = AlignUp(BootInfo->Kernel.Base + BootInfo->Kernel.Size, PAGE_SIZE);

    for (UINTN index = 0; index < EntryCount; index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = GetMemoryDescriptor(MemoryMap, index);

        if (Descriptor->Type != EFI_CONVENTIONAL_MEMORY)
        {
            continue;
        }

        BuddyAddUsableRangeExcept(Descriptor->PhysicalStart, Descriptor->NumberOfPages, KernelStart, KernelEnd);
    }

    return TRUE;
}

UINT64 KGetTotalPages(VOID)
{
    return TotalPages;
}

VOID *KMemMove(VOID *destination, CONST VOID *source, UINTN size)
{
    UINT8 *dst = (UINT8 *) destination;
    CONST UINT8 *src = (CONST UINT8 *) source;

    if (dst == src || size == 0)
    {
        return destination;
    }

    if (dst < src)
    {
        for (UINTN i = 0; i < size; i++)
        {
            dst[i] = src[i];
        }
    }
    else
    {
        for (UINTN i = size; i > 0; i--)
        {
            dst[i - 1] = src[i - 1];
        }
    }

    return destination;
}

VOID *KMemCpy(VOID *destination, CONST VOID *source, UINTN size)
{
    UINT8 *dst = (UINT8 *) destination;
    CONST UINT8 *src = (CONST UINT8 *) source;

    for (UINTN index = 0; index < size; index++)
    {
        dst[index] = src[index];
    }

    return destination;
}

VOID *KMemSet(VOID *destination, UINT8 value, UINTN size)
{
    UINT8 *dst = (UINT8 *) destination;

    for (UINTN index = 0; index < size; index++)
    {
        dst[index] = value;
    }

    return destination;
}

VOID *KAllocPages(UINTN PageCount)
{
    UINTN order;
    UINT64 flags;
    VOID *address;

    if (PageCount == 0)
    {
        return NULL;
    }

    order = PTOO(PageCount);

    if (order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    flags = SpinLockAcquireIRQSave(&BuddyLock);
    address = BuddyAllocate(order);
    SpinLockReleaseIRQRestore(&BuddyLock, flags);

    if (address == NULL)
    {
        return NULL;
    }

    KMemSet(address, 0, (UINTN) OrderToSize(order));

    return address;
}

VOID *KAllocPagesBelow(UINTN PageCount, UINT64 limit)
{
    UINTN order;
    UINT64 flags;
    VOID *address;

    if (PageCount == 0 || limit == 0)
    {
        return NULL;
    }

    order = PTOO(PageCount);

    if (order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    flags = SpinLockAcquireIRQSave(&BuddyLock);
    address = BuddyAllocateBelow(order, limit);
    SpinLockReleaseIRQRestore(&BuddyLock, flags);

    if (address == NULL)
    {
        return NULL;
    }

    KMemSet(address, 0, (UINTN) OrderToSize(order));

    return address;
}

VOID KMemFreePages(VOID *address, UINTN PageCount)
{
    UINT64 Address;
    UINTN order;
    UINT64 flags;

    if (address == NULL || PageCount == 0)
    {
        return;
    }

    Address = (UINT64) address;

    if ((Address & (PAGE_SIZE - 1)) != 0)
    {
        return;
    }

    order = PTOO(PageCount);

    if (order > BUDDY_MAX_ORDER)
    {
        return;
    }

    flags = SpinLockAcquireIRQSave(&BuddyLock);
    BuddyFree(Address, order);
    SpinLockReleaseIRQRestore(&BuddyLock, flags);
}

STATIC VOID HeapInitializeBlock(HEAP_BLOCK *block, UINTN size, BOOLEAN free)
{
    block->Size = size;
    block->Free = free;
    block->Next = NULL;
    block->Previous = NULL;
}

STATIC HEAP_BLOCK *HeapFindFreeBlock(UINTN size)
{
    HEAP_BLOCK *block = HeapHead;

    while (block != NULL)
    {
        if (block->Free && block->Size >= size)
        {
            return block;
        }

        block = block->Next;
    }

    return NULL;
}

STATIC VOID HeapSplitBlock(HEAP_BLOCK *block, UINTN size)
{
    HEAP_BLOCK *NewBlock;
    UINTN RemainingSize;

    if (block->Size < size)
    {
        return;
    }

    RemainingSize = block->Size - size;

    if (RemainingSize < sizeof(HEAP_BLOCK) + HEAP_MIN_BLOCK_SIZE)
    {
        return;
    }

    NewBlock = (HEAP_BLOCK *) ((UINT8 *) (block + 1) + size);

    HeapInitializeBlock(NewBlock, RemainingSize - sizeof(HEAP_BLOCK), TRUE);
    NewBlock->Next = block->Next;
    NewBlock->Previous = block;

    if (block->Next != NULL)
    {
        block->Next->Previous = NewBlock;
    }
    else
    {
        HeapTail = NewBlock;
    }

    block->Next = NewBlock;
    block->Size = size;
}

STATIC BOOLEAN HeapBlocksAdjacent(HEAP_BLOCK *FirstBlock, HEAP_BLOCK *SecondBlock)
{
    return (UINT8 *) (FirstBlock + 1) + FirstBlock->Size == (UINT8 *) SecondBlock;
}

STATIC VOID HeapMergeWithNext(HEAP_BLOCK *block)
{
    HEAP_BLOCK *NextBlock;

    if (block == NULL || block->Next == NULL)
    {
        return;
    }

    NextBlock = block->Next;

    if (!NextBlock->Free)
    {
        return;
    }

    if (!HeapBlocksAdjacent(block, NextBlock))
    {
        return;
    }

    block->Size += sizeof(HEAP_BLOCK) + NextBlock->Size;
    block->Next = NextBlock->Next;

    if (NextBlock->Next != NULL)
    {
        NextBlock->Next->Previous = block;
    }
    else
    {
        HeapTail = block;
    }
}

STATIC HEAP_BLOCK *HeapExpand(UINTN MinimumSize)
{
    UINTN RequiredSize;
    UINTN PageCount;
    UINTN order;
    UINTN AllocatedPages;
    UINTN AllocatedSize;
    VOID *memory;
    HEAP_BLOCK *block;

    RequiredSize = MinimumSize + sizeof(HEAP_BLOCK);
    PageCount = (UINTN) (AlignUp(RequiredSize, PAGE_SIZE) / PAGE_SIZE);

    if (PageCount < HEAP_EXPAND_PAGES)
    {
        PageCount = HEAP_EXPAND_PAGES;
    }

    order = PTOO(PageCount);

    if (order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    AllocatedPages = (UINTN) OrderToPages(order);
    AllocatedSize = AllocatedPages * PAGE_SIZE;

    memory = KAllocPages(AllocatedPages);

    if (memory == NULL)
    {
        return NULL;
    }

    block = (HEAP_BLOCK *) memory;
    HeapInitializeBlock(block, AllocatedSize - sizeof(HEAP_BLOCK), TRUE);

    if (HeapHead == NULL)
    {
        HeapHead = block;
        HeapTail = block;
        return block;
    }

    block->Previous = HeapTail;
    HeapTail->Next = block;
    HeapTail = block;

    if (block->Previous->Free && HeapBlocksAdjacent(block->Previous, block))
    {
        block = block->Previous;
        HeapMergeWithNext(block);
    }

    return block;
}

BOOLEAN KHeapInit(VOID)
{
    if (HeapInitialized)
    {
        return TRUE;
    }

    HeapHead = NULL;
    HeapTail = NULL;

    if (HeapExpand(HEAP_INITIAL_PAGES * PAGE_SIZE - sizeof(HEAP_BLOCK)) == NULL)
    {
        return FALSE;
    }

    HeapInitialized = TRUE;

    return TRUE;
}

VOID *KAlloc(UINTN size)
{
    HEAP_BLOCK *block;
    UINT64 flags;

    if (size == 0 || !HeapInitialized)
    {
        return NULL;
    }

    size = (UINTN) AlignUp(size, HEAP_ALIGNMENT);

    flags = SpinLockAcquireIRQSave(&HeapLock);

    block = HeapFindFreeBlock(size);

    if (block == NULL)
    {
        block = HeapExpand(size);

        if (block == NULL)
        {
            SpinLockReleaseIRQRestore(&HeapLock, flags);
            return NULL;
        }
    }

    HeapSplitBlock(block, size);
    block->Free = FALSE;

    SpinLockReleaseIRQRestore(&HeapLock, flags);

    return (VOID *) (block + 1);
}

VOID KFree(VOID *address)
{
    HEAP_BLOCK *block;
    UINT64 flags;

    if (address == NULL || !HeapInitialized)
    {
        return;
    }

    flags = SpinLockAcquireIRQSave(&HeapLock);

    block = ((HEAP_BLOCK *) address) - 1;
    block->Free = TRUE;

    HeapMergeWithNext(block);

    if (block->Previous != NULL && block->Previous->Free)
    {
        block = block->Previous;
        HeapMergeWithNext(block);
    }

    SpinLockReleaseIRQRestore(&HeapLock, flags);
}
