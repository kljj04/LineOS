// kernel/src/memory/memory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>
#include <memory/memory.h>

#define PAGE_SIZE                 4096ULL
#define BITS_PER_BYTE             8ULL
#define LOW_MEMORY_RESERVED_PAGES 256ULL

STATIC UINT8 *PageBitmap = NULL;
STATIC UINT64 PageBitmapSize = 0;
STATIC UINT64 TotalPages = 0;
STATIC UINT64 LastSearchPage = 0;

STATIC UINT64 AlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

STATIC UINT64 PageIndexFromAddress(UINT64 Address)
{
    return Address / PAGE_SIZE;
}

STATIC UINT64 AddressFromPageIndex(UINT64 PageIndex)
{
    return PageIndex * PAGE_SIZE;
}

STATIC EFI_MEMORY_DESCRIPTOR *GetMemoryDescriptor(LINEOS_MEMORY_MAP *MemoryMap, UINTN Index)
{
    return (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *) MemoryMap->MemoryMap + (Index * MemoryMap->MemoryMapDescriptorSize));
}

STATIC UINTN GetMemoryDescriptorCount(LINEOS_MEMORY_MAP *MemoryMap)
{
    return MemoryMap->MemoryMapSize / MemoryMap->MemoryMapDescriptorSize;
}

STATIC VOID BitmapSet(UINT64 PageIndex)
{
    PageBitmap[PageIndex / BITS_PER_BYTE] |= (UINT8) (1U << (PageIndex % BITS_PER_BYTE));
}

STATIC VOID BitmapClear(UINT64 PageIndex)
{
    PageBitmap[PageIndex / BITS_PER_BYTE] &= (UINT8) ~(1U << (PageIndex % BITS_PER_BYTE));
}

STATIC BOOLEAN BitmapTest(UINT64 PageIndex)
{
    return (PageBitmap[PageIndex / BITS_PER_BYTE] & (UINT8) (1U << (PageIndex % BITS_PER_BYTE))) != 0;
}

STATIC UINT64 FindHighestConventionalMemoryAddress(LINEOS_MEMORY_MAP *MemoryMap)
{
    UINT64 HighestAddress = 0;
    UINTN  EntryCount = GetMemoryDescriptorCount(MemoryMap);

    for (UINTN Index = 0; Index < EntryCount; Index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = GetMemoryDescriptor(MemoryMap, Index);
        UINT64                 End = Descriptor->PhysicalStart + (Descriptor->NumberOfPages * PAGE_SIZE);

        if (Descriptor->Type != EFI_CONVENTIONAL_MEMORY)
        {
            continue;
        }

        if (End > HighestAddress)
        {
            HighestAddress = End;
        }
    }

    return HighestAddress;
}

STATIC EFI_MEMORY_DESCRIPTOR *FindBitmapStorageDescriptor(LINEOS_MEMORY_MAP *MemoryMap, UINT64 BitmapBytes)
{
    EFI_MEMORY_DESCRIPTOR *BestDescriptor = NULL;
    UINT64                 BestPages = 0;
    UINTN                  EntryCount = GetMemoryDescriptorCount(MemoryMap);
    UINT64                 NeededPages = AlignUp(BitmapBytes, PAGE_SIZE) / PAGE_SIZE;

    for (UINTN Index = 0; Index < EntryCount; Index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = GetMemoryDescriptor(MemoryMap, Index);

        if (Descriptor->Type != EFI_CONVENTIONAL_MEMORY || Descriptor->NumberOfPages < NeededPages)
        {
            continue;
        }

        if (Descriptor->NumberOfPages > BestPages)
        {
            BestDescriptor = Descriptor;
            BestPages = Descriptor->NumberOfPages;
        }
    }

    return BestDescriptor;
}

STATIC VOID MarkPagesUsed(UINT64 StartPage, UINT64 PageCount)
{
    for (UINT64 Index = 0; Index < PageCount && StartPage + Index < TotalPages; Index++)
    {
        BitmapSet(StartPage + Index);
    }
}

STATIC VOID MarkPagesFree(UINT64 StartPage, UINT64 PageCount)
{
    for (UINT64 Index = 0; Index < PageCount && StartPage + Index < TotalPages; Index++)
    {
        BitmapClear(StartPage + Index);
    }
}

STATIC VOID MarkConventionalMemoryFree(LINEOS_MEMORY_MAP *MemoryMap)
{
    UINTN EntryCount = GetMemoryDescriptorCount(MemoryMap);

    for (UINTN Index = 0; Index < EntryCount; Index++)
    {
        EFI_MEMORY_DESCRIPTOR *Descriptor = GetMemoryDescriptor(MemoryMap, Index);

        if (Descriptor->Type != EFI_CONVENTIONAL_MEMORY)
        {
            continue;
        }

        MarkPagesFree(PageIndexFromAddress(Descriptor->PhysicalStart), Descriptor->NumberOfPages);
    }
}

STATIC BOOLEAN ArePagesFree(UINT64 StartPage, UINTN PageCount)
{
    if (StartPage + PageCount > TotalPages)
    {
        return FALSE;
    }

    for (UINTN Index = 0; Index < PageCount; Index++)
    {
        if (BitmapTest(StartPage + Index))
        {
            return FALSE;
        }
    }

    return TRUE;
}

STATIC UINT64 FindFreePageRange(UINTN PageCount)
{
    UINT64 StartPage = LastSearchPage;

    for (UINT64 Pass = 0; Pass < 2; Pass++)
    {
        for (UINT64 Page = StartPage; Page + PageCount <= TotalPages; Page++)
        {
            if (ArePagesFree(Page, PageCount))
            {
                LastSearchPage = Page + PageCount;
                return Page;
            }
        }

        StartPage = 0;
    }

    return TotalPages;
}

BOOLEAN KMemoryInit(LINEOS_BOOT_INFO *BootInfo)
{
    LINEOS_MEMORY_MAP     *MemoryMap;
    EFI_MEMORY_DESCRIPTOR *BitmapStorage;
    UINT64                 HighestAddress;
    UINT64                 BitmapPages;

    if (BootInfo == NULL || BootInfo->MemoryMap == NULL || BootInfo->MemoryMap->MemoryMap == NULL || BootInfo->MemoryMap->MemoryMapDescriptorSize == 0)
    {
        return FALSE;
    }

    MemoryMap = BootInfo->MemoryMap;
    HighestAddress = FindHighestConventionalMemoryAddress(MemoryMap);
    TotalPages = AlignUp(HighestAddress, PAGE_SIZE) / PAGE_SIZE;
    PageBitmapSize = AlignUp(TotalPages, BITS_PER_BYTE) / BITS_PER_BYTE;
    BitmapStorage = FindBitmapStorageDescriptor(MemoryMap, PageBitmapSize);

    if (TotalPages == 0 || PageBitmapSize == 0 || BitmapStorage == NULL)
    {
        return FALSE;
    }

    BitmapPages = AlignUp(PageBitmapSize, PAGE_SIZE) / PAGE_SIZE;
    PageBitmap = (UINT8 *) (BitmapStorage->PhysicalStart + ((BitmapStorage->NumberOfPages - BitmapPages) * PAGE_SIZE));

    KMemSet(PageBitmap, 0xFF, (UINTN) PageBitmapSize);
    MarkConventionalMemoryFree(MemoryMap);
    MarkPagesUsed(0, LOW_MEMORY_RESERVED_PAGES);
    MarkPagesUsed(PageIndexFromAddress(BootInfo->Kernel.Base), AlignUp(BootInfo->Kernel.Size, PAGE_SIZE) / PAGE_SIZE);
    MarkPagesUsed(PageIndexFromAddress((UINT64) PageBitmap), BitmapPages);
    LastSearchPage = LOW_MEMORY_RESERVED_PAGES;

    return TRUE;
}

VOID *KGetPageBitmap(VOID)
{
    return PageBitmap;
}

UINT64 KGetPageBitmapSize(VOID)
{
    return PageBitmapSize;
}

UINT64 KGetTotalPages(VOID)
{
    return TotalPages;
}

UINT8 KGetPageBitmapByte(UINT64 ByteIndex)
{
    if (PageBitmap == NULL || ByteIndex >= PageBitmapSize)
    {
        return 0;
    }

    return PageBitmap[ByteIndex];
}

VOID *KMemMove(VOID *destination, CONST VOID *source, UINTN size)
{
    UINT8       *dst = (UINT8 *) destination;
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
    UINT8       *dst = (UINT8 *) destination;
    CONST UINT8 *src = (CONST UINT8 *) source;

    for (UINTN Index = 0; Index < size; Index++)
    {
        dst[Index] = src[Index];
    }

    return destination;
}

VOID *KMemSet(VOID *destination, UINT8 value, UINTN size)
{
    UINT8 *dst = (UINT8 *) destination;

    for (UINTN Index = 0; Index < size; Index++)
    {
        dst[Index] = value;
    }

    return destination;
}

VOID *KAllocPages(UINTN pageCount)
{
    UINT64 Page;

    if (pageCount == 0 || PageBitmap == NULL)
    {
        return NULL;
    }

    Page = FindFreePageRange(pageCount);
    if (Page == TotalPages)
    {
        return NULL;
    }

    MarkPagesUsed(Page, pageCount);
    KMemSet((VOID *) AddressFromPageIndex(Page), 0, (UINTN) (pageCount * PAGE_SIZE));

    return (VOID *) AddressFromPageIndex(Page);
}

VOID KMemFreePages(VOID *address, UINTN pageCount)
{
    UINT64 Address;

    if (address == NULL || pageCount == 0 || PageBitmap == NULL)
    {
        return;
    }

    Address = (UINT64) address;
    if ((Address % PAGE_SIZE) != 0)
    {
        return;
    }

    MarkPagesFree(PageIndexFromAddress(Address), pageCount);
}
