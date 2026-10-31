// Kernel/Source/Memory/PMM.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Arch/AMD64/SpinLock.h>
#include <LineOS/BootInfo.h>
#include <Memory/PMM.h>

#define PAGE_SIZE                 4096ULL
#define LOW_MEMORY_RESERVED_PAGES 256ULL
#define BUDDY_MAX_ORDER           20

#define HEAP_ALIGNMENT      16ULL
#define HEAP_INITIAL_PAGES  16ULL
#define HEAP_EXPAND_PAGES   16ULL
#define HEAP_MIN_BLOCK_SIZE 16ULL

STATIC BUDDY_BLOCK *FreeLists[BUDDY_MAX_ORDER + 1];
STATIC UINT64       TotalPages = 0;
STATIC UINT64       ManagedPages;
STATIC UINT64       FreePages;
STATIC SPIN_LOCK    BuddyLock;

STATIC HEAP_BLOCK *HeapHead = NULL;
STATIC HEAP_BLOCK *HeapTail = NULL;
STATIC BOOLEAN     HeapInitialized = FALSE;
STATIC SPIN_LOCK   HeapLock;

STATIC UINT64 AlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

STATIC UINT64 AlignDown(UINT64 Value, UINT64 Alignment)
{
    return Value & ~(Alignment - 1);
}

STATIC UINTN PToO(UINTN PageCount)
{
    UINTN Order = 0;
    UINTN Pages = 1;

    while (Pages < PageCount)
    {
        Pages <<= 1;
        Order++;
    }

    return Order;
}

STATIC UINT64 OToP(UINTN Order)
{
    return 1ULL << Order;
}

STATIC UINT64 OToS(UINTN Order)
{
    return PAGE_SIZE << Order;
}

STATIC EFI_MEMORY_DESCRIPTOR *GetMemoryDescriptor(LINEOS_MEMORY_MAP *MemoryMap, UINTN Index)
{
    return (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *) MemoryMap->MemoryMap + (Index * MemoryMap->MemoryMapDescriptorSize));
}

STATIC UINTN GetMemoryDescriptorCount(LINEOS_MEMORY_MAP *MemoryMap)
{
    return MemoryMap->MemoryMapSize / MemoryMap->MemoryMapDescriptorSize;
}

STATIC UINT64 FindHighestConventionalMemoryAddress(LINEOS_MEMORY_MAP *MemoryMap)
{
    UINT64 HighestAddress = 0;
    UINTN  EntryCount = GetMemoryDescriptorCount(MemoryMap);

    for (UINTN Index = 0; Index < EntryCount; Index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = GetMemoryDescriptor(MemoryMap, Index);
        UINT64                 End;

        if (Descriptor->Type != EFI_CONVENTIONAL_MEMORY)
        {
            continue;
        }

        End = Descriptor->PhysicalStart + (Descriptor->NumberOfPages * PAGE_SIZE);

        if (End > HighestAddress)
        {
            HighestAddress = End;
        }
    }

    return HighestAddress;
}

STATIC VOID BuddyPush(UINTN Order, UINT64 Address)
{
    BUDDY_BLOCK *Block;

    if (Order > BUDDY_MAX_ORDER)
    {
        return;
    }

    Block = (BUDDY_BLOCK *) Address;
    Block->Next = FreeLists[Order];
    FreeLists[Order] = Block;
    FreePages += 1ULL << Order;
}

STATIC BUDDY_BLOCK *BuddyPop(UINTN Order)
{
    BUDDY_BLOCK *Block;

    if (Order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    Block = FreeLists[Order];

    if (Block == NULL)
    {
        return NULL;
    }

    FreeLists[Order] = Block->Next;
    FreePages -= 1ULL << Order;
    Block->Next = NULL;

    return Block;
}

STATIC BOOLEAN BuddyRemove(UINTN Order, UINT64 Address)
{
    BUDDY_BLOCK *Block;
    BUDDY_BLOCK *Previous = NULL;

    if (Order > BUDDY_MAX_ORDER)
    {
        return FALSE;
    }

    Block = FreeLists[Order];

    while (Block != NULL)
    {
        if ((UINT64) Block == Address)
        {
            if (Previous == NULL)
            {
                FreeLists[Order] = Block->Next;
            }
            else
            {
                Previous->Next = Block->Next;
            }

            FreePages -= 1ULL << Order;
            Block->Next = NULL;

            return TRUE;
        }

        Previous = Block;
        Block = Block->Next;
    }

    return FALSE;
}

STATIC UINTN LargestOrderForRange(UINT64 Address, UINT64 PageCount)
{
    UINTN Order = 0;

    while (Order < BUDDY_MAX_ORDER)
    {
        UINTN  NextOrder = Order + 1;
        UINT64 NextPages = OToP(NextOrder);
        UINT64 NextSize = OToS(NextOrder);

        if (NextPages > PageCount)
        {
            break;
        }

        if ((Address & (NextSize - 1)) != 0)
        {
            break;
        }

        Order = NextOrder;
    }

    return Order;
}

STATIC VOID BuddyAddRange(UINT64 Address, UINT64 PageCount)
{
    while (PageCount != 0)
    {
        UINTN  Order = LargestOrderForRange(Address, PageCount);
        UINT64 Pages = OToP(Order);

        BuddyPush(Order, Address);

        Address += Pages * PAGE_SIZE;
        PageCount -= Pages;
    }
}

STATIC VOID BuddyAddUsableRange(UINT64 Address, UINT64 PageCount)
{
    UINT64 Start;
    UINT64 End;
    UINT64 ReservedEnd = LOW_MEMORY_RESERVED_PAGES * PAGE_SIZE;

    if (PageCount == 0)
    {
        return;
    }

    Start = AlignUp(Address, PAGE_SIZE);
    End = AlignDown(Address + (PageCount * PAGE_SIZE), PAGE_SIZE);

    if (End <= Start)
    {
        return;
    }

    if (End <= ReservedEnd)
    {
        return;
    }

    if (Start < ReservedEnd)
    {
        Start = ReservedEnd;
    }

    if (End <= Start)
    {
        return;
    }

    BuddyAddRange(Start, (End - Start) / PAGE_SIZE);
}

STATIC VOID BuddyAddUsableRangeExcept(UINT64 Address, UINT64 PageCount, UINT64 ReservedStart, UINT64 ReservedEnd)
{
    UINT64 Start;
    UINT64 End;

    if (PageCount == 0)
    {
        return;
    }

    Start = AlignUp(Address, PAGE_SIZE);
    End = AlignDown(Address + (PageCount * PAGE_SIZE), PAGE_SIZE);

    if (End <= Start)
    {
        return;
    }

    if (ReservedEnd <= ReservedStart || End <= ReservedStart || Start >= ReservedEnd)
    {
        BuddyAddUsableRange(Start, (End - Start) / PAGE_SIZE);

        return;
    }

    if (Start < ReservedStart)
    {
        BuddyAddUsableRange(Start, (ReservedStart - Start) / PAGE_SIZE);
    }

    if (End > ReservedEnd)
    {
        BuddyAddUsableRange(ReservedEnd, (End - ReservedEnd) / PAGE_SIZE);
    }
}

STATIC VOID *BuddyAllocate(UINTN Order)
{
    UINTN        CurrentOrder;
    BUDDY_BLOCK *Block;
    UINT64       Address;

    if (Order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    CurrentOrder = Order;

    while (CurrentOrder <= BUDDY_MAX_ORDER && FreeLists[CurrentOrder] == NULL)
    {
        CurrentOrder++;
    }

    if (CurrentOrder > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    Block = BuddyPop(CurrentOrder);

    if (Block == NULL)
    {
        return NULL;
    }

    Address = (UINT64) Block;

    while (CurrentOrder > Order)
    {
        UINT64 BuddyAddress;

        CurrentOrder--;
        BuddyAddress = Address + OToS(CurrentOrder);

        BuddyPush(CurrentOrder, BuddyAddress);
    }

    return (VOID *) Address;
}

STATIC VOID *BuddyAllocateBelow(UINTN Order, UINT64 Limit)
{
    if (Order > BUDDY_MAX_ORDER || Limit == 0)
    {
        return NULL;
    }

    for (UINTN CurrentOrder = Order; CurrentOrder <= BUDDY_MAX_ORDER; CurrentOrder++)
    {
        BUDDY_BLOCK *Block = FreeLists[CurrentOrder];

        while (Block != NULL)
        {
            UINT64 Address = (UINT64) Block;
            UINT64 RequestedSize = OToS(Order);

            if (Address <= Limit && RequestedSize <= Limit - Address)
            {
                if (!BuddyRemove(CurrentOrder, Address))
                {
                    return NULL;
                }

                while (CurrentOrder > Order)
                {
                    UINT64 BuddyAddress;

                    CurrentOrder--;
                    BuddyAddress = Address + OToS(CurrentOrder);

                    BuddyPush(CurrentOrder, BuddyAddress);
                }

                return (VOID *) Address;
            }

            Block = Block->Next;
        }
    }

    return NULL;
}

STATIC VOID BuddyFree(UINT64 Address, UINTN Order)
{
    if (Order > BUDDY_MAX_ORDER)
    {
        return;
    }

    while (Order < BUDDY_MAX_ORDER)
    {
        UINT64 Size = OToS(Order);
        UINT64 BuddyAddress = Address ^ Size;

        if (!BuddyRemove(Order, BuddyAddress))
        {
            break;
        }

        if (BuddyAddress < Address)
        {
            Address = BuddyAddress;
        }

        Order++;
    }

    BuddyPush(Order, Address);
}

BOOLEAN KMemoryInit(LINEOS_BOOT_INFO *BootInfo)
{
    LINEOS_MEMORY_MAP *MemoryMap;
    UINTN              EntryCount;
    UINT64             HighestAddress;
    UINT64             KernelStart;
    UINT64             KernelEnd;

    if (BootInfo == NULL || BootInfo->MemoryMap == NULL || BootInfo->MemoryMap->MemoryMap == NULL || BootInfo->MemoryMap->MemoryMapDescriptorSize == 0)
    {
        return FALSE;
    }

    ManagedPages = 0;
    FreePages = 0;
    SpinLockInit(&BuddyLock);
    SpinLockInit(&HeapLock);

    for (UINTN Order = 0; Order <= BUDDY_MAX_ORDER; Order++)
    {
        FreeLists[Order] = NULL;
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

    for (UINTN Index = 0; Index < EntryCount; Index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = GetMemoryDescriptor(MemoryMap, Index);

        if (Descriptor->Type != EFI_CONVENTIONAL_MEMORY)
        {
            continue;
        }

        BuddyAddUsableRangeExcept(Descriptor->PhysicalStart, Descriptor->NumberOfPages, KernelStart, KernelEnd);
    }

    ManagedPages = FreePages;

    return TRUE;
}

UINT64 KGetTotalPages(VOID)
{
    return TotalPages;
}

VOID *KMemMove(VOID *Destination, CONST VOID *Source, UINTN Size)
{
    UINT8       *Dst = (UINT8 *) Destination;
    CONST UINT8 *Src = (CONST UINT8 *) Source;

    if (Dst == Src || Size == 0)
    {
        return Destination;
    }

    if (Dst < Src)
    {
        for (UINTN I = 0; I < Size; I++)
        {
            Dst[I] = Src[I];
        }
    }
    else
    {
        for (UINTN I = Size; I > 0; I--)
        {
            Dst[I - 1] = Src[I - 1];
        }
    }

    return Destination;
}

VOID *KMemCpy(VOID *Destination, CONST VOID *Source, UINTN Size)
{
    VOID       *Target = Destination;
    CONST VOID *Input = Source;

    ASM("cld; rep movsb" : "+D"(Target), "+S"(Input), "+c"(Size) : : "memory");

    return Destination;
}

VOID *KMemSet(VOID *Destination, UINT8 Value, UINTN Size)
{
    UINT8 *Dst = (UINT8 *) Destination;

    for (UINTN Index = 0; Index < Size; Index++)
    {
        Dst[Index] = Value;
    }

    return Destination;
}

VOID *KAllocPages(UINTN PageCount)
{
    UINTN  Order;
    UINT64 Flags;
    VOID  *Address;

    if (PageCount == 0)
    {
        return NULL;
    }

    Order = PToO(PageCount);

    if (Order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    Flags = SpinLockAcquireIRQSave(&BuddyLock);
    Address = BuddyAllocate(Order);
    SpinLockReleaseIRQRestore(&BuddyLock, Flags);

    if (Address == NULL)
    {
        return NULL;
    }

    KMemSet(Address, 0, (UINTN) OToS(Order));

    return Address;
}

VOID *KAllocPagesBelow(UINTN PageCount, UINT64 Limit)
{
    UINTN  Order;
    UINT64 Flags;
    VOID  *Address;

    if (PageCount == 0 || Limit == 0)
    {
        return NULL;
    }

    Order = PToO(PageCount);

    if (Order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    Flags = SpinLockAcquireIRQSave(&BuddyLock);
    Address = BuddyAllocateBelow(Order, Limit);
    SpinLockReleaseIRQRestore(&BuddyLock, Flags);

    if (Address == NULL)
    {
        return NULL;
    }

    KMemSet(Address, 0, (UINTN) OToS(Order));

    return Address;
}

VOID KMemFreePages(VOID *PageAddress, UINTN PageCount)
{
    UINT64 Address;
    UINTN  Order;
    UINT64 Flags;

    if (PageAddress == NULL || PageCount == 0)
    {
        return;
    }

    Address = (UINT64) PageAddress;

    if ((Address & (PAGE_SIZE - 1)) != 0)
    {
        return;
    }

    Order = PToO(PageCount);

    if (Order > BUDDY_MAX_ORDER)
    {
        return;
    }

    Flags = SpinLockAcquireIRQSave(&BuddyLock);
    BuddyFree(Address, Order);
    SpinLockReleaseIRQRestore(&BuddyLock, Flags);
}

STATIC VOID HeapInitializeBlock(HEAP_BLOCK *Block, UINTN Size, BOOLEAN Free)
{
    Block->Size = Size;
    Block->Free = Free;
    Block->Next = NULL;
    Block->Previous = NULL;
}

STATIC HEAP_BLOCK *HeapFindFreeBlock(UINTN Size)
{
    HEAP_BLOCK *Block = HeapHead;

    while (Block != NULL)
    {
        if (Block->Free && Block->Size >= Size)
        {
            return Block;
        }

        Block = Block->Next;
    }

    return NULL;
}

STATIC VOID HeapSplitBlock(HEAP_BLOCK *Block, UINTN Size)
{
    HEAP_BLOCK *NewBlock;
    UINTN       RemainingSize;

    if (Block->Size < Size)
    {
        return;
    }

    RemainingSize = Block->Size - Size;

    if (RemainingSize < sizeof(HEAP_BLOCK) + HEAP_MIN_BLOCK_SIZE)
    {
        return;
    }

    NewBlock = (HEAP_BLOCK *) ((UINT8 *) (Block + 1) + Size);

    HeapInitializeBlock(NewBlock, RemainingSize - sizeof(HEAP_BLOCK), TRUE);
    NewBlock->Next = Block->Next;
    NewBlock->Previous = Block;

    if (Block->Next != NULL)
    {
        Block->Next->Previous = NewBlock;
    }
    else
    {
        HeapTail = NewBlock;
    }

    Block->Next = NewBlock;
    Block->Size = Size;
}

STATIC BOOLEAN HeapBlocksAdjacent(HEAP_BLOCK *FirstBlock, HEAP_BLOCK *SecondBlock)
{
    return (UINT8 *) (FirstBlock + 1) + FirstBlock->Size == (UINT8 *) SecondBlock;
}

STATIC VOID HeapMergeWithNext(HEAP_BLOCK *Block)
{
    HEAP_BLOCK *NextBlock;

    if (Block == NULL || Block->Next == NULL)
    {
        return;
    }

    NextBlock = Block->Next;

    if (!NextBlock->Free)
    {
        return;
    }

    if (!HeapBlocksAdjacent(Block, NextBlock))
    {
        return;
    }

    Block->Size += sizeof(HEAP_BLOCK) + NextBlock->Size;
    Block->Next = NextBlock->Next;

    if (NextBlock->Next != NULL)
    {
        NextBlock->Next->Previous = Block;
    }
    else
    {
        HeapTail = Block;
    }
}

STATIC HEAP_BLOCK *HeapExpand(UINTN MinimumSize)
{
    UINTN       RequiredSize;
    UINTN       PageCount;
    UINTN       Order;
    UINTN       AllocatedPages;
    UINTN       AllocatedSize;
    VOID       *Memory;
    HEAP_BLOCK *Block;

    RequiredSize = MinimumSize + sizeof(HEAP_BLOCK);
    PageCount = (UINTN) (AlignUp(RequiredSize, PAGE_SIZE) / PAGE_SIZE);

    if (PageCount < HEAP_EXPAND_PAGES)
    {
        PageCount = HEAP_EXPAND_PAGES;
    }

    Order = PToO(PageCount);

    if (Order > BUDDY_MAX_ORDER)
    {
        return NULL;
    }

    AllocatedPages = (UINTN) OToP(Order);
    AllocatedSize = AllocatedPages * PAGE_SIZE;

    Memory = KAllocPages(AllocatedPages);

    if (Memory == NULL)
    {
        return NULL;
    }

    Block = (HEAP_BLOCK *) Memory;
    HeapInitializeBlock(Block, AllocatedSize - sizeof(HEAP_BLOCK), TRUE);

    if (HeapHead == NULL)
    {
        HeapHead = Block;
        HeapTail = Block;

        return Block;
    }

    Block->Previous = HeapTail;
    HeapTail->Next = Block;
    HeapTail = Block;

    if (Block->Previous->Free && HeapBlocksAdjacent(Block->Previous, Block))
    {
        Block = Block->Previous;
        HeapMergeWithNext(Block);
    }

    return Block;
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

VOID *KAlloc(UINTN Size)
{
    HEAP_BLOCK *Block;
    UINT64      Flags;

    if (Size == 0 || !HeapInitialized)
    {
        return NULL;
    }

    Size = (UINTN) AlignUp(Size, HEAP_ALIGNMENT);

    Flags = SpinLockAcquireIRQSave(&HeapLock);

    Block = HeapFindFreeBlock(Size);

    if (Block == NULL)
    {
        Block = HeapExpand(Size);

        if (Block == NULL)
        {
            SpinLockReleaseIRQRestore(&HeapLock, Flags);

            return NULL;
        }
    }

    HeapSplitBlock(Block, Size);
    Block->Free = FALSE;

    SpinLockReleaseIRQRestore(&HeapLock, Flags);

    return (VOID *) (Block + 1);
}

VOID KFree(VOID *Address)
{
    HEAP_BLOCK *Block;
    UINT64      Flags;

    if (Address == NULL || !HeapInitialized)
    {
        return;
    }

    Flags = SpinLockAcquireIRQSave(&HeapLock);

    Block = ((HEAP_BLOCK *) Address) - 1;
    Block->Free = TRUE;

    HeapMergeWithNext(Block);

    if (Block->Previous != NULL && Block->Previous->Free)
    {
        Block = Block->Previous;
        HeapMergeWithNext(Block);
    }

    SpinLockReleaseIRQRestore(&HeapLock, Flags);
}

BOOLEAN KMemoryPanicLocksAvailable(VOID)
{
    return ATOMIC_LOAD(&HeapLock.Locked, ATOMIC_ACQUIRE) == 0 && ATOMIC_LOAD(&BuddyLock.Locked, ATOMIC_ACQUIRE) == 0;
}

VOID KGetManagedMemory(UINT64 *Managed, UINT64 *Free)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&BuddyLock);

    *Managed = ManagedPages * PAGE_SIZE;
    *Free = FreePages * PAGE_SIZE;
    SpinLockReleaseIRQRestore(&BuddyLock, Flags);
}
