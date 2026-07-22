// memory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>
#include <memory/memory.h>

#define PAGE_SIZE 4096ULL
#define HEAP_BLOCK_MAGIC 0x48454150424C4B31ULL

typedef struct HeapBlock
{
    UINT64 Magic;
    UINTN Size;
    BOOLEAN Free;
    struct HeapBlock *Next;
} HEAP_BLOCK;

LINEOS_MEMORY_INFO MemoryInfo;
LINEOS_PMM_INFO PMMInfo;
STATIC LINEOS_MEMORY_MAP *MemoryMap = NULL;
STATIC LINEOS_RSDP *RSDP = NULL;
STATIC HEAP_BLOCK *HeapHead = NULL;

STATIC EFI_MEMORY_DESCRIPTOR *MemoryMapDescriptorAt(UINTN index)
{
    UINT8 *base;

    if (MemoryMap == NULL || index >= MemoryInfo.DescriptorCount)
    {
        return NULL;
    }

    base = (UINT8 *) MemoryMap->MemoryMap;
    return (EFI_MEMORY_DESCRIPTOR *) (base + (index * MemoryMap->MemoryMapDescriptorSize));
}

BOOLEAN MemoryTypeIsUsable(UINT32 type)
{
    return type == EfiConventionalMemory;
}

BOOLEAN MemoryTypeIsMMIO(UINT32 type)
{
    return type == EfiMemoryMappedIO || type == EfiMemoryMappedIOPortSpace;
}

EFI_MEMORY_DESCRIPTOR *MemoryMapGetDescriptor(UINTN index)
{
    return MemoryMapDescriptorAt(index);
}

VOID MemoryMapInit(LINEOS_BOOT_INFO *BootInfo)
{
    MemSet(&MemoryInfo, 0, sizeof(LINEOS_MEMORY_INFO));

    if (BootInfo == NULL || BootInfo->MemoryMap == NULL || BootInfo->MemoryMap->MemoryMap == NULL || BootInfo->MemoryMap
        ->MemoryMapDescriptorSize == 0)
    {
        return;
    }

    MemoryMap = BootInfo->MemoryMap;
    MemoryInfo.DescriptorCount = MemoryMap->MemoryMapSize / MemoryMap->MemoryMapDescriptorSize;

    for (UINTN index = 0; index < MemoryInfo.DescriptorCount; index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = MemoryMapDescriptorAt(index);
        UINT64 BaseAddress = Descriptor->PhysicalStart;
        UINT64 size = Descriptor->NumberOfPages * 4096ULL;
        UINT64 EndAddress = BaseAddress + size;

        if (EndAddress > MemoryInfo.HighestPhysicalAddress)
        {
            MemoryInfo.HighestPhysicalAddress = EndAddress;
        }

        if (MemoryTypeIsUsable(Descriptor->Type))
        {
            MemoryInfo.UsableTotalSize += size;

            if (size > MemoryInfo.UsableSize)
            {
                MemoryInfo.UsableBase = BaseAddress;
                MemoryInfo.UsableSize = size;
            }

            continue;
        }

        if (MemoryTypeIsMMIO(Descriptor->Type))
        {
            MemoryInfo.MMIOTotalSize += size;
            continue;
        }

        if (Descriptor->Type == EfiACPIReclaimMemory || Descriptor->Type == EfiACPIMemoryNVS)
        {
            MemoryInfo.ACPITotalSize += size;
            continue;
        }

        MemoryInfo.ReservedTotalSize += size;
    }
}

STATIC UINT64 AlignUp(UINT64 value, UINT64 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

STATIC UINT64 AlignDown(UINT64 value, UINT64 alignment)
{
    return value & ~(alignment - 1);
}

STATIC VOID BitmapSet(UINT64 index)
{
    PMMInfo.Bitmap[index / 8] |= (UINT8) (1 << (index % 8));
}

STATIC VOID BitmapClear(UINT64 index)
{
    PMMInfo.Bitmap[index / 8] &= (UINT8) ~(1 << (index % 8));
}

STATIC BOOLEAN BitmapTest(UINT64 index)
{
    return (PMMInfo.Bitmap[index / 8] & (UINT8) (1 << (index % 8))) != 0;
}

STATIC VOID PMMMarkFree(UINT64 address, UINT64 count)
{
    UINT64 index;

    if (address < PMMInfo.Base)
    {
        return;
    }

    index = (address - PMMInfo.Base) / PAGE_SIZE;

    for (UINT64 i = 0; i < count && index + i < PMMInfo.PageCount; i++)
    {
        if (BitmapTest(index + i))
        {
            BitmapClear(index + i);
            PMMInfo.FreePageCount++;
            PMMInfo.UsedPageCount--;
        }
    }
}

STATIC VOID PMMMarkUsed(UINT64 address, UINT64 count)
{
    UINT64 index;

    if (address < PMMInfo.Base)
    {
        return;
    }

    index = (address - PMMInfo.Base) / PAGE_SIZE;

    for (UINT64 i = 0; i < count && index + i < PMMInfo.PageCount; i++)
    {
        if (!BitmapTest(index + i))
        {
            BitmapSet(index + i);
            PMMInfo.FreePageCount--;
            PMMInfo.UsedPageCount++;
        }
    }
}

VOID PMMInit(LINEOS_BOOT_INFO *BootInfo)
{
    UINT64 BitmapPages;
    UINT64 BitmapAddress;

    MemoryMapInit(BootInfo);
    RSDP = (LINEOS_RSDP *) BootInfo->RSDP;

    MemSet(&PMMInfo, 0, sizeof(LINEOS_PMM_INFO));

    if (MemoryInfo.UsableSize == 0)
    {
        return;
    }

    PMMInfo.Base = AlignDown(MemoryInfo.UsableBase, PAGE_SIZE);
    PMMInfo.PageCount = MemoryInfo.UsableSize / PAGE_SIZE;
    PMMInfo.BitmapSize = AlignUp(PMMInfo.PageCount, 8) / 8;
    BitmapPages = AlignUp(PMMInfo.BitmapSize, PAGE_SIZE) / PAGE_SIZE;
    BitmapAddress = AlignUp(MemoryInfo.UsableBase, PAGE_SIZE);
    PMMInfo.Bitmap = (UINT8 *) BitmapAddress;

    MemSet(PMMInfo.Bitmap, 0xFF, PMMInfo.BitmapSize);
    PMMInfo.FreePageCount = 0;
    PMMInfo.UsedPageCount = PMMInfo.PageCount;

    for (UINTN index = 0; index < MemoryInfo.DescriptorCount; index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = MemoryMapDescriptorAt(index);
        UINT64 BaseAddress = AlignUp(Descriptor->PhysicalStart, PAGE_SIZE);
        UINT64 EndAddress = AlignDown(Descriptor->PhysicalStart + (Descriptor->NumberOfPages * PAGE_SIZE), PAGE_SIZE);

        if (MemoryTypeIsUsable(Descriptor->Type) && EndAddress > BaseAddress)
        {
            PMMMarkFree(BaseAddress, (EndAddress - BaseAddress) / PAGE_SIZE);
        }
    }

    PMMMarkUsed(BitmapAddress, BitmapPages);
}

VOID *PMMAllocPages(UINTN count)
{
    UINT64 run = 0;
    UINT64 StartIndex = 0;

    if (count == 0 || PMMInfo.Bitmap == NULL)
    {
        return NULL;
    }

    for (UINT64 index = 0; index < PMMInfo.PageCount; index++)
    {
        if (BitmapTest(index))
        {
            run = 0;
            continue;
        }

        if (run == 0)
        {
            StartIndex = index;
        }

        run++;

        if (run == count)
        {
            UINT64 address = PMMInfo.Base + (StartIndex * PAGE_SIZE);
            PMMMarkUsed(address, count);
            return (VOID *) address;
        }
    }

    return NULL;
}

VOID *PMMAllocPage(VOID)
{
    return PMMAllocPages(1);
}

VOID PMMFreePages(VOID *address, UINTN count)
{
    if (address == NULL || count == 0)
    {
        return;
    }

    PMMMarkFree((UINT64) address, count);
}

VOID PMMFreePage(VOID *address)
{
    PMMFreePages(address, 1);
}

STATIC UINTN HeapAlign(UINTN size)
{
    return (size + 15) & ~(UINTN) 15;
}

STATIC HEAP_BLOCK *HeapNewBlock(UINTN size)
{
    UINTN TotalSize = sizeof(HEAP_BLOCK) + size;
    UINTN PageCount = (UINTN) AlignUp(TotalSize, PAGE_SIZE) / PAGE_SIZE;
    HEAP_BLOCK *block = (HEAP_BLOCK *) PMMAllocPages(PageCount);

    if (block == NULL)
    {
        return NULL;
    }

    block->Magic = HEAP_BLOCK_MAGIC;
    block->Size = (PageCount * PAGE_SIZE) - sizeof(HEAP_BLOCK);
    block->Free = FALSE;
    block->Next = NULL;
    return block;
}

STATIC VOID HeapSplitBlock(HEAP_BLOCK *block, UINTN size)
{
    HEAP_BLOCK *NextBlock;

    if (block->Size < size + sizeof(HEAP_BLOCK) + 16)
    {
        return;
    }

    NextBlock = (HEAP_BLOCK *) ((UINT8 *) (block + 1) + size);
    NextBlock->Magic = HEAP_BLOCK_MAGIC;
    NextBlock->Size = block->Size - size - sizeof(HEAP_BLOCK);
    NextBlock->Free = TRUE;
    NextBlock->Next = block->Next;

    block->Size = size;
    block->Next = NextBlock;
}

VOID *KMalloc(UINTN size)
{
    HEAP_BLOCK *block;

    if (size == 0)
    {
        return NULL;
    }

    size = HeapAlign(size);
    block = HeapHead;

    while (block != NULL)
    {
        if (block->Free && block->Size >= size)
        {
            block->Free = FALSE;
            HeapSplitBlock(block, size);
            return block + 1;
        }

        if (block->Next == NULL)
        {
            break;
        }

        block = block->Next;
    }

    HEAP_BLOCK *NewBlock = HeapNewBlock(size);

    if (NewBlock == NULL)
    {
        return NULL;
    }

    HeapSplitBlock(NewBlock, size);

    if (HeapHead == NULL)
    {
        HeapHead = NewBlock;
    }
    else
    {
        block->Next = NewBlock;
    }

    return NewBlock + 1;
}

VOID KFree(VOID *ptr)
{
    HEAP_BLOCK *block;

    if (ptr == NULL)
    {
        return;
    }

    block = ((HEAP_BLOCK *) ptr) - 1;

    if (block->Magic != HEAP_BLOCK_MAGIC)
    {
        return;
    }

    block->Free = TRUE;

    while (block->Next != NULL && block->Next->Free)
    {
        block->Size += sizeof(HEAP_BLOCK) + block->Next->Size;
        block->Next = block->Next->Next;
    }
}

VOID *KRealloc(VOID *ptr, UINTN size)
{
    HEAP_BLOCK *block;
    VOID *NewPtr;

    if (ptr == NULL)
    {
        return KMalloc(size);
    }

    if (size == 0)
    {
        KFree(ptr);
        return NULL;
    }

    block = ((HEAP_BLOCK *) ptr) - 1;

    if (block->Magic != HEAP_BLOCK_MAGIC)
    {
        return NULL;
    }

    size = HeapAlign(size);

    if (block->Size >= size)
    {
        HeapSplitBlock(block, size);
        return ptr;
    }

    NewPtr = KMalloc(size);

    if (NewPtr == NULL)
    {
        return NULL;
    }

    MemCopy(NewPtr, ptr, block->Size);
    KFree(ptr);
    return NewPtr;
}

LINEOS_RSDP *ACPIGetRSDP(VOID)
{
    return RSDP;
}

UINT8 MMIORead8(UINT64 address)
{
    return *(volatile UINT8 *) address;
}

UINT16 MMIORead16(UINT64 address)
{
    return *(volatile UINT16 *) address;
}

UINT32 MMIORead32(UINT64 address)
{
    return *(volatile UINT32 *) address;
}

UINT64 MMIORead64(UINT64 address)
{
    return *(volatile UINT64 *) address;
}

VOID MMIOWrite8(UINT64 address, UINT8 value)
{
    *(volatile UINT8 *) address = value;
}

VOID MMIOWrite16(UINT64 address, UINT16 value)
{
    *(volatile UINT16 *) address = value;
}

VOID MMIOWrite32(UINT64 address, UINT32 value)
{
    *(volatile UINT32 *) address = value;
}

VOID MMIOWrite64(UINT64 address, UINT64 value)
{
    *(volatile UINT64 *) address = value;
}

VOID *MemSet(VOID *destination, UINT8 value, UINTN size)
{
    UINT8 *Destination8 = (UINT8 *) destination;
    UINT64 Pattern = value;

    Pattern |= Pattern << 8;
    Pattern |= Pattern << 16;
    Pattern |= Pattern << 32;

    while ((((UINTN) Destination8) & 7) != 0 && size != 0)
    {
        *Destination8++ = value;
        size--;
    }

    UINT64 *Destination64 = (UINT64 *) Destination8;

    while (size >= sizeof(UINT64))
    {
        *Destination64++ = Pattern;
        size -= sizeof(UINT64);
    }

    Destination8 = (UINT8 *) Destination64;

    while (size != 0)
    {
        *Destination8++ = value;
        size--;
    }

    return destination;
}

VOID *MemCopy(VOID *destination, CONST VOID *source, UINTN size)
{
    UINT8 *Destination8 = (UINT8 *) destination;
    CONST UINT8 *Source8 = (CONST UINT8 *) source;

    while ((((UINTN) Destination8) & 7) != 0 && size != 0)
    {
        *Destination8++ = *Source8++;
        size--;
    }

    if ((((UINTN) Source8) & 7) == 0)
    {
        UINT64 *Destination64 = (UINT64 *) Destination8;
        CONST UINT64 *Source64 = (CONST UINT64 *) Source8;

        while (size >= sizeof(UINT64))
        {
            *Destination64++ = *Source64++;
            size -= sizeof(UINT64);
        }

        Destination8 = (UINT8 *) Destination64;
        Source8 = (CONST UINT8 *) Source64;
    }

    while (size != 0)
    {
        *Destination8++ = *Source8++;
        size--;
    }

    return destination;
}

VOID *MemMove(VOID *destination, CONST VOID *source, UINTN size)
{
    UINT8 *Destination = (UINT8 *) destination;
    CONST UINT8 *Source = (CONST UINT8 *) source;

    if (Destination == Source || size == 0)
    {
        return destination;
    }

    if (Destination < Source || Destination >= Source + size)
    {
        MemCopy(destination, source, size);
    }
    else
    {
        Destination += size;
        Source += size;

        while ((((UINTN) Destination) & 7) != 0 && size != 0)
        {
            *--Destination = *--Source;
            size--;
        }

        if ((((UINTN) Source) & 7) == 0)
        {
            UINT64 *Destination64 = (UINT64 *) Destination;
            CONST UINT64 *Source64 = (CONST UINT64 *) Source;

            while (size >= sizeof(UINT64))
            {
                *--Destination64 = *--Source64;
                size -= sizeof(UINT64);
            }

            Destination = (UINT8 *) Destination64;
            Source = (CONST UINT8 *) Source64;
        }

        while (size != 0)
        {
            *--Destination = *--Source;
            size--;
        }
    }

    return destination;
}

INT32 MemCompare(CONST VOID *first, CONST VOID *second, UINTN size)
{
    CONST UINT8 *First = (CONST UINT8 *) first;
    CONST UINT8 *Second = (CONST UINT8 *) second;

    for (UINTN index = 0; index < size; index++)
    {
        if (First[index] != Second[index])
        {
            return (INT32) First[index] - (INT32) Second[index];
        }
    }

    return 0;
}
