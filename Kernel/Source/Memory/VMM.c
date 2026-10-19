// Kernel/Source/Memory/VMM.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Arch/AMD64/CPU.h>
#include <Arch/AMD64/SpinLock.h>
#include <Memory/PMM.h>
#include <Memory/VMM.h>
#include <Multicore/SMP.h>
#include <Interrupt/APIC.h>
#include <Timer/TSC.h>

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

STATIC UINT64  VMMGeneration;
STATIC UINT64  VMMRequest[SMP_MAX_CPUS];
STATIC UINT64  VMMAck[SMP_MAX_CPUS];
STATIC UINT64  VMMAddress;
STATIC BOOLEAN VMMFailed;
STATIC BOOLEAN VMMInitialized;

STATIC BOOLEAN VMMLock(UINT64 *IRQFlags)
{
    ASM("pushfq; popq %0; cli" : "=r"(*IRQFlags) : : "memory");

    if (__atomic_exchange_n(&VMMMapLock.Locked, 1, __ATOMIC_ACQUIRE) == 0)
    {
        return TRUE;
    }

    if ((*IRQFlags & (1ULL << 9)) != 0)
    {
        ASM("sti" ::: "memory");
    }

    return FALSE;
}

STATIC BOOLEAN VMMCanModify(VOID)
{
    UINT64 CR4;

    ASM("mov %%cr4, %0" : "=r"(CR4));
    return VMMInitialized && !VMMFailed && SMPIsReady() && VMMGetCurrentCR3() == SMPGetBootCR3() && (CR4 & ((1ULL << 12) | (1ULL << 17))) == 0;
}

VOID VMMHandleShootdown(VOID)
{
    UINT32 CPUID = SMPGetCurrentCPUID();
    UINT64 Generation;
    UINT64 CR4;

    ASM("mov %%cr4, %0" : "=r"(CR4));

    if (CPUID >= SMP_MAX_CPUS || (CR4 & ((1ULL << 12) | (1ULL << 17))) != 0)
    {
        return;
    }

    Generation = __atomic_load_n(&VMMRequest[CPUID], __ATOMIC_ACQUIRE);

    if (Generation != __atomic_load_n(&VMMAck[CPUID], __ATOMIC_RELAXED) && VMMGetCurrentCR3() == SMPGetBootCR3())
    {
        ASM("invlpg (%0)" : : "r"(VMMAddress) : "memory");
        __atomic_store_n(&VMMAck[CPUID], Generation, __ATOMIC_RELEASE);
    }
}

STATIC BOOLEAN VMMShootdown(UINT64 VirtAddr)
{
    UINT32  CurrentCPUID = SMPGetCurrentCPUID();
    UINT32  CPUCount = SMPGetCPUCount();
    UINT64  Start;
    UINT64  Timeout = TSCGetFrequency() / 10;
    BOOLEAN Result = TRUE;

    ASM("invlpg (%0)" : : "r"(VirtAddr) : "memory");
    VMMAddress = VirtAddr;
    VMMGeneration++;

    if (CurrentCPUID >= CPUCount || CPUCount > SMP_MAX_CPUS || Timeout == 0 || VMMGeneration == 0)
    {
        VMMFailed = TRUE;
        return FALSE;
    }

    for (UINT32 Index = 0; Index < CPUCount; Index++)
    {
        CPU_INFO *CPU = SMPGetCPU(Index);

        if (Index == CurrentCPUID || CPU == NULL || !__atomic_load_n(&CPU->Online, __ATOMIC_ACQUIRE))
        {
            continue;
        }

        __atomic_store_n(&VMMRequest[Index], VMMGeneration, __ATOMIC_RELEASE);

        if (!LAPICSendIPI(CPU->APICID, 70))
        {
            Result = FALSE;
        }
    }

    Start = RDTSC();

    for (UINT32 Index = 0; Index < CPUCount; Index++)
    {
        if (__atomic_load_n(&VMMRequest[Index], __ATOMIC_RELAXED) != VMMGeneration)
        {
            continue;
        }

        while (__atomic_load_n(&VMMAck[Index], __ATOMIC_ACQUIRE) != VMMGeneration)
        {
            if (RDTSC() - Start >= Timeout)
            {
                VMMFailed = TRUE;
                return FALSE;
            }

            ASM("pause" ::: "memory");
        }
    }

    VMMFailed = !Result;
    return Result;
}

UINT64 VMMGetCurrentCR3(VOID)
{
    UINT64 CR3;

    ASM("mov %%cr3, %0" : "=r"(CR3));
    return CR3;
}

STATIC BOOLEAN VMMTranslateUnlocked(UINT64 VirtAddr, UINT64 *PhysAddr)
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

BOOLEAN VMMTranslate(UINT64 VirtAddr, UINT64 *PhysAddr)
{
    UINT64  IRQFlags;
    BOOLEAN Result;

    if (!VMMLock(&IRQFlags))
    {
        return FALSE;
    }

    Result = VMMTranslateUnlocked(VirtAddr, PhysAddr);
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

STATIC BOOLEAN VMMTableWritable(UINT64 Address)
{
    VOLATILE UINT64 *Table = (VOLATILE UINT64 *) (VMMGetCurrentCR3() & VMM_PAGE_MASK);
    UINT64           Entry;
    UINT64           PhysAddr;

    if (!VMMTranslateUnlocked(Address, &PhysAddr) || PhysAddr != Address)
    {
        return FALSE;
    }

    for (UINT32 Level = 0; Level < 4; Level++)
    {
        Entry = Table[(Address >> (39 - Level * 9)) & 0x1FF];

        if ((Entry & (VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE)) != (VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE))
        {
            return FALSE;
        }

        if (Level == 3 || (Level != 0 && (Entry & VMM_PAGE_LARGE) != 0))
        {
            return TRUE;
        }

        Table = (VOLATILE UINT64 *) (Entry & VMM_PAGE_MASK);
    }

    return FALSE;
}

STATIC VOID VMMFreeClone(UINT64 *Table, UINT32 Level)
{
    if (Level < 3)
    {
        for (UINT32 Index = 0; Index < 512; Index++)
        {
            UINT64 Entry = Table[Index];

            if ((Entry & VMM_PAGE_PRESENT) != 0 && (Entry & VMM_PAGE_LARGE) == 0)
            {
                VMMFreeClone((UINT64 *) (Entry & VMM_PAGE_MASK), Level + 1);
            }
        }
    }

    KMemFreePages(Table, 1);
}

STATIC UINT64 *VMMCloneTable(UINT64 Address, UINT32 Level, UINT64 PhysMask)
{
    UINT64 *Source = (UINT64 *) Address;
    UINT64 *Table;
    UINT64  PhysAddr;

    if (!VMMTranslateUnlocked(Address, &PhysAddr) || PhysAddr != Address)
    {
        return NULL;
    }

    Table = KAllocPagesBelow(1, 0x100000000ULL);

    if (Table == NULL)
    {
        return NULL;
    }

    if (((UINT64) Table & ~PhysMask) != 0 || !VMMTableWritable((UINT64) Table))
    {
        KMemFreePages(Table, 1);
        return NULL;
    }

    for (UINT32 Index = 0; Index < 512; Index++)
    {
        UINT64 Entry = Source[Index];

        if (Level < 3 && (Entry & VMM_PAGE_PRESENT) != 0 && (Entry & VMM_PAGE_LARGE) == 0)
        {
            UINT64 *Child = VMMCloneTable(Entry & VMM_PAGE_MASK, Level + 1, PhysMask);

            if (Child == NULL)
            {
                VMMFreeClone(Table, Level);
                return NULL;
            }

            Entry = (Entry & ~VMM_PAGE_MASK) | (UINT64) Child;
        }

        Table[Index] = Entry;
    }

    return Table;
}

BOOLEAN VMMInit(VOID)
{
    UINT64  IRQFlags;
    UINT64  PhysMask;
    UINT64  CR4;
    UINT64  CR3;
    UINT64 *Root;
    BOOLEAN Result = FALSE;

    if (!VMMLock(&IRQFlags))
    {
        return FALSE;
    }

    ASM("mov %%cr4, %0" : "=r"(CR4));
    CR3 = VMMGetCurrentCR3();

    if (VMMInitialized || SMPGetCPUCount() != 0 || (ReadMSR(0x1B) & (1ULL << 10)) != 0 || (CR4 & ((1ULL << 12) | (1ULL << 17))) != 0 || !VMMGetPhysicalAddressMask(&PhysMask))
    {
        goto Cleanup;
    }

    Root = VMMCloneTable(CR3 & VMM_PAGE_MASK, 0, PhysMask);

    if (Root != NULL)
    {
        CR3 = (UINT64) Root | (CR3 & 0x18ULL);
        ASM("mov %0, %%cr3" : : "r"(CR3) : "memory");
        VMMInitialized = TRUE;
        Result = TRUE;
    }

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

BOOLEAN VMMMapPage(UINT64 VirtAddr, UINT64 PhysAddr, UINT64 Flags)
{
    VOLATILE UINT64 *Table;
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

    if (!VMMGetPhysicalAddressMask(&PhysMask) || (PhysAddr & ~PhysMask) != 0)
    {
        return FALSE;
    }

    ASM("mov %%cr4, %0" : "=r"(CR4));

    if ((CR4 & VMM_CR4_LA57) != 0)
    {
        return FALSE;
    }

    if (!VMMLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMCanModify())
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
        if (!VMMTableWritable((UINT64) Table))
        {
            goto Cleanup;
        }

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

    if (!VMMTableWritable((UINT64) Table))
    {
        goto Cleanup;
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

            if (((UINT64) NewTables[Level] & 0xFFFULL) != 0 || ((UINT64) NewTables[Level] & ~PhysMask) != 0 || !VMMTableWritable((UINT64) NewTables[Level]))
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

        ((VOLATILE UINT64 *) NewTables[2])[Indices[3]] = PhysAddr | VMM_PAGE_PRESENT | Flags;

        for (UINT32 Level = FirstMissingLevel; Level < 2; Level++)
        {
            ((VOLATILE UINT64 *) NewTables[Level])[Indices[Level + 1]] = (UINT64) NewTables[Level + 1] | ParentFlags;
        }

        __atomic_store_n(&Table[Indices[FirstMissingLevel]], (UINT64) NewTables[FirstMissingLevel] | ParentFlags, __ATOMIC_RELEASE);

        for (UINT32 Level = FirstMissingLevel; Level < 3; Level++)
        {
            NewTables[Level] = NULL;
        }
    }
    else
    {
        __atomic_store_n(&Table[Indices[3]], PhysAddr | VMM_PAGE_PRESENT | Flags, __ATOMIC_RELEASE);
    }

    Result = VMMShootdown(VirtAddr);

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

BOOLEAN VMMUnmapPage(UINT64 VirtAddr, UINT64 *PhysAddr)
{
    VOLATILE UINT64 *Table;
    VOLATILE UINT64 *Entry;
    UINT64           EntryValue;
    UINT64           IRQFlags;
    BOOLEAN          Result = FALSE;

    if (PhysAddr == NULL || !VMMIsCanonicalAddress(VirtAddr) || (VirtAddr & 0xFFFULL) != 0 || !VMMLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMCanModify())
    {
        goto Cleanup;
    }

    Table = (VOLATILE UINT64 *) (VMMGetCurrentCR3() & VMM_PAGE_MASK);

    for (UINT32 Level = 0; Level < 3; Level++)
    {
        EntryValue = Table[(VirtAddr >> (39 - Level * 9)) & 0x1FF];

        if ((EntryValue & VMM_PAGE_PRESENT) == 0 || (EntryValue & VMM_PAGE_LARGE) != 0)
        {
            goto Cleanup;
        }

        Table = (VOLATILE UINT64 *) (EntryValue & VMM_PAGE_MASK);
    }

    if (!VMMTableWritable((UINT64) Table))
    {
        goto Cleanup;
    }

    Entry = &Table[(VirtAddr >> 12) & 0x1FF];
    EntryValue = *Entry;

    if ((EntryValue & VMM_PAGE_PRESENT) == 0 || (EntryValue & VMM_PAGE_MASK) == VirtAddr)
    {
        goto Cleanup;
    }

    __atomic_store_n(Entry, 0, __ATOMIC_RELEASE);

    if (VMMShootdown(VirtAddr))
    {
        *PhysAddr = EntryValue & VMM_PAGE_MASK;
        Result = TRUE;
    }

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}
