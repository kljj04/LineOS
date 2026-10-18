// Kernel/Source/Memory/VMM.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Arch/AMD64/CPU.h>
#include <Arch/AMD64/SpinLock.h>
#include <Memory/PMM.h>
#include <Memory/VMM.h>
#include <Multicore/SMP.h>

#define VMM_PAGE_SIZE     4096ULL
#define VMM_PAGE_MASK     0x000FFFFFFFFFF000ULL
#define VMM_PAGE_PRESENT  (1ULL << 0)
#define VMM_PAGE_WRITABLE (1ULL << 1)
#define VMM_PAGE_USER     (1ULL << 2)
#define VMM_PAGE_LARGE    (1ULL << 7)
#define VMM_CR4_LA57      (1ULL << 12)
#define VMM_LARGE_2M_MASK 0x000FFFFFFFE00000ULL
#define VMM_LARGE_1G_MASK 0x000FFFFFC0000000ULL

STATIC SPIN_LOCK VMMMapLock;

STATIC BOOLEAN VMMIsCanonicalAddress(UINT64 VirtAddr)
{
    UINT64  UpperBits;
    BOOLEAN SignBit;

    UpperBits = VirtAddr >> 48;
    SignBit = (VirtAddr & (1ULL << 47)) != 0;

    if (SignBit)
    {
        return UpperBits == 0xFFFF;
    }

    return UpperBits == 0;
}

STATIC BOOLEAN VMMGetPhysicalAddressMask(UINT64 *PhysMask)
{
    UINT32 EAX;
    UINT32 EBX;
    UINT32 ECX;
    UINT32 EDX;
    UINT32 MaxLeaf;
    UINT32 PhysBits;

    if (PhysMask == NULL)
    {
        return FALSE;
    }

    CPUID(0x80000000U, 0, &EAX, &EBX, &ECX, &EDX);
    MaxLeaf = EAX;

    if (MaxLeaf < 0x80000008U)
    {
        return FALSE;
    }

    CPUID(0x80000008U, 0, &EAX, &EBX, &ECX, &EDX);
    PhysBits = EAX & 0xFFU;

    if (PhysBits < 32 || PhysBits > 52)
    {
        return FALSE;
    }

    *PhysMask = ((1ULL << PhysBits) - 1) & VMM_PAGE_MASK;
    return TRUE;
}

STATIC BOOLEAN VMMHasOnlyCurrentCPUOnline(VOID)
{
    UINT32 CurrentCPUID;
    UINT32 OnlineCount = 0;
    UINT32 CPUCount;

    CurrentCPUID = SMPGetCurrentCPUID();
    CPUCount = SMPGetCPUCount();

    if (CurrentCPUID >= SMP_MAX_CPUS || CPUCount == 0 || CPUCount > SMP_MAX_CPUS)
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < CPUCount; Index++)
    {
        CPU_INFO *CPU = SMPGetCPU(Index);

        if (CPU == NULL || !CPU->Online)
        {
            continue;
        }

        if (CPU->CPUID != CurrentCPUID)
        {
            return FALSE;
        }

        OnlineCount++;
    }

    return OnlineCount == 1;
}

UINT64 VMMGetCurrentCR3(VOID)
{
    UINT64 CR3;

    ASM("mov %%cr3, %0" : "=r"(CR3));
    return CR3;
}

BOOLEAN VMMTranslate(UINT64 VirtAddr, UINT64 *PhysAddr)
{
    VOLATILE UINT64 *PML4;
    VOLATILE UINT64 *PDPT;
    VOLATILE UINT64 *PD;
    VOLATILE UINT64 *PT;
    UINT64           CR4;
    UINT64           Entry;

    if (PhysAddr == NULL || !VMMIsCanonicalAddress(VirtAddr))
    {
        return FALSE;
    }

    ASM("mov %%cr4, %0" : "=r"(CR4));

    if ((CR4 & VMM_CR4_LA57) != 0)
    {
        return FALSE;
    }

    PML4 = (VOLATILE UINT64 *) (VMMGetCurrentCR3() & VMM_PAGE_MASK);
    Entry = PML4[(VirtAddr >> 39) & 0x1FF];

    if ((Entry & VMM_PAGE_PRESENT) == 0)
    {
        return FALSE;
    }

    PDPT = (VOLATILE UINT64 *) (Entry & VMM_PAGE_MASK);
    Entry = PDPT[(VirtAddr >> 30) & 0x1FF];

    if ((Entry & VMM_PAGE_PRESENT) == 0)
    {
        return FALSE;
    }

    if ((Entry & VMM_PAGE_LARGE) != 0)
    {
        *PhysAddr = (Entry & VMM_LARGE_1G_MASK) | (VirtAddr & 0x3FFFFFFFULL);
        return TRUE;
    }

    PD = (VOLATILE UINT64 *) (Entry & VMM_PAGE_MASK);
    Entry = PD[(VirtAddr >> 21) & 0x1FF];

    if ((Entry & VMM_PAGE_PRESENT) == 0)
    {
        return FALSE;
    }

    if ((Entry & VMM_PAGE_LARGE) != 0)
    {
        *PhysAddr = (Entry & VMM_LARGE_2M_MASK) | (VirtAddr & 0x1FFFFFULL);
        return TRUE;
    }

    PT = (VOLATILE UINT64 *) (Entry & VMM_PAGE_MASK);
    Entry = PT[(VirtAddr >> 12) & 0x1FF];

    if ((Entry & VMM_PAGE_PRESENT) == 0)
    {
        return FALSE;
    }

    *PhysAddr = (Entry & VMM_PAGE_MASK) | (VirtAddr & (VMM_PAGE_SIZE - 1));
    return TRUE;
}

BOOLEAN VMMMapPage(UINT64 VirtAddr, UINT64 PhysAddr, UINT64 Flags)
{
    VOLATILE UINT64 *Table;
    VOLATILE UINT64 *NextTable;
    VOLATILE UINT64 *Entry;
    VOID            *NewTables[3] = {NULL, NULL, NULL};
    UINT64           PhysMask;
    UINT64           CR4;
    UINT64           IRQFlags;
    UINT64           EntryValue;
    UINT64           ParentFlags;
    UINT64           Indices[4];
    UINT32           FirstMissingLevel = 4;
    BOOLEAN          Result = FALSE;

    if (!VMMIsCanonicalAddress(VirtAddr) || (VirtAddr & 0xFFFULL) != 0 || (PhysAddr & 0xFFFULL) != 0)
    {
        return FALSE;
    }

    if ((Flags & ~(VMM_FLAG_WRITABLE | VMM_FLAG_USER)) != 0)
    {
        return FALSE;
    }

    for (UINTN I = 0; I < 3; I++)

        if (!VMMGetPhysicalAddressMask(&PhysMask) || (PhysAddr & ~PhysMask) != 0)
        {
            return FALSE;
        }

    ASM("mov %%cr4, %0" : "=r"(CR4));

    if ((CR4 & VMM_CR4_LA57) != 0)
    {
        return FALSE;
    }

    IRQFlags = SpinLockAcquireIRQSave(&VMMMapLock);

    if (!VMMHasOnlyCurrentCPUOnline())
    {
        goto Cleanup;
    }

    Indices[0] = (VirtAddr >> 39) & 0x1FF;
    Indices[1] = (VirtAddr >> 30) & 0x1FF;
    Indices[2] = (VirtAddr >> 21) & 0x1FF;
    Indices[3] = (VirtAddr >> 12) & 0x1FF;

    Table = (VOLATILE UINT64 *) (VMMGetCurrentCR3() & VMM_PAGE_MASK);

    for (UINT32 Level = 0; Level < 3; Level++)
    {
        Entry = &Table[Indices[Level]];
        EntryValue = *Entry;

        if ((EntryValue & VMM_PAGE_PRESENT) == 0)
        {
            FirstMissingLevel = Level;
            break;
        }

        if ((EntryValue & VMM_PAGE_LARGE) != 0 && Level != 0)
        {
            goto Cleanup;
        }

        if ((Flags & VMM_FLAG_WRITABLE) != 0 && (EntryValue & VMM_PAGE_WRITABLE) == 0)
        {
            goto Cleanup;
        }

        if ((Flags & VMM_FLAG_USER) != 0 && (EntryValue & VMM_PAGE_USER) == 0)
        {
            goto Cleanup;
        }

        Table = (VOLATILE UINT64 *) (EntryValue & VMM_PAGE_MASK);
    }

    if (FirstMissingLevel == 4 && (Table[Indices[3]] & VMM_PAGE_PRESENT) != 0)
    {
        goto Cleanup;
    }

    if (FirstMissingLevel != 4)
    {
        for (UINT32 Level = FirstMissingLevel; Level < 3; Level++)
        {
            NewTables[Level] = KAllocPages(1);

            if (NewTables[Level] == NULL)
            {
                goto Cleanup;
            }

            if (((UINT64) NewTables[Level] & 0xFFFULL) != 0 || ((UINT64) NewTables[Level] & ~PhysMask) != 0)
            {
                goto Cleanup;
            }

            KMemSet(NewTables[Level], 0, VMM_PAGE_SIZE);
        }

        ParentFlags = VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE;

        if ((Flags & VMM_FLAG_USER) != 0)
        {
            ParentFlags |= VMM_PAGE_USER;
        }

        for (UINT32 Level = FirstMissingLevel; Level < 3; Level++)
        {
            Entry = &Table[Indices[Level]];
            NextTable = (VOLATILE UINT64 *) NewTables[Level];
            *Entry = ((UINT64) NextTable & PhysMask) | ParentFlags;
            Table = NextTable;
            NewTables[Level] = NULL;
        }
    }

    Table[Indices[3]] = PhysAddr | VMM_PAGE_PRESENT | Flags;
    ASM("invlpg (%0)" : : "r"(VirtAddr) : "memory");
    Result = TRUE;

Cleanup:
    for (UINT32 Level = 0; Level < 3; Level++)
    {
        if (NewTables[Level] != NULL)
        {
            KMemFreePages(NewTables[Level], 1);
        }
    }

    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}