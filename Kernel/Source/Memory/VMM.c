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
#define VMM_PAGE_GLOBAL   (1ULL << 8)
#define VMM_PAGE_NX       (1ULL << 63)
#define VMM_CR4_LA57      (1ULL << 12)
#define VMM_LARGE_2M_MASK 0x000FFFFFFFE00000ULL
#define VMM_LARGE_1G_MASK 0x000FFFFFC0000000ULL

STATIC SPIN_LOCK VMMMapLock;

STATIC UINT64 *VMMAllocateAddressSpacePage(UINT64 PhysMask);
STATIC BOOLEAN VMMKernelMappingsIdle(VOID);

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

    if (ATOMIC_EXCHANGE(&VMMMapLock.Locked, 1, ATOMIC_ACQUIRE) == 0)
    {
        return TRUE;
    }

    if ((*IRQFlags & (1ULL << 9)) != 0)
    {
        ASM("sti" ::: "memory");
    }

    return FALSE;
}

STATIC BOOLEAN VMMWaitLock(UINT64 *IRQFlags)
{
    ASM("pushfq; popq %0; cli" : "=r"(*IRQFlags) : : "memory");

    while (ATOMIC_EXCHANGE(&VMMMapLock.Locked, 1, ATOMIC_ACQUIRE) != 0)
    {
        VMMHandleShootdown();
        ASM("pause" ::: "memory");
    }

    return TRUE;
}

STATIC BOOLEAN VMMCanModify(VOID)
{
    UINT64 CR4;

    ASM("mov %%cr4, %0" : "=r"(CR4));

    return VMMInitialized && !VMMFailed && SMPIsReady() && (CR4 & ((1ULL << 12) | (1ULL << 17))) == 0;
}

STATIC BOOLEAN VMMCanModifyCurrentRoot(VOID)
{
    return VMMCanModify() && VMMGetCurrentCR3() == SMPGetBootCR3();
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

    Generation = ATOMIC_LOAD(&VMMRequest[CPUID], ATOMIC_ACQUIRE);

    if (Generation != ATOMIC_LOAD(&VMMAck[CPUID], ATOMIC_RELAXED) && VMMGetCurrentCR3() == SMPGetBootCR3())
    {
        ASM("invlpg (%0)" : : "r"(VMMAddress) : "memory");
        ATOMIC_STORE(&VMMAck[CPUID], Generation, ATOMIC_RELEASE);
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

        if (Index == CurrentCPUID || CPU == NULL || !ATOMIC_LOAD(&CPU->Online, ATOMIC_ACQUIRE))
        {
            continue;
        }

        ATOMIC_STORE(&VMMRequest[Index], VMMGeneration, ATOMIC_RELEASE);

        if (!LAPICSendIPI(CPU->APICID, 70))
        {
            Result = FALSE;
        }
    }

    Start = RDTSC();

    for (UINT32 Index = 0; Index < CPUCount; Index++)
    {
        if (ATOMIC_LOAD(&VMMRequest[Index], ATOMIC_RELAXED) != VMMGeneration)
        {
            continue;
        }

        while (ATOMIC_LOAD(&VMMAck[Index], ATOMIC_ACQUIRE) != VMMGeneration)
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

STATIC BOOLEAN VMMTranslateRoot(UINT64 *Root, UINT64 VirtAddr, UINT64 *PhysAddr)
{
    VOLATILE UINT64 *PML4;
    VOLATILE UINT64 *PDPT;
    VOLATILE UINT64 *PD;
    VOLATILE UINT64 *PT;
    UINT64           Entry;

    if (PhysAddr == NULL || !VMMIsCanonicalAddress(VirtAddr))
    {
        return FALSE;
    }

    PML4 = Root;
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

STATIC BOOLEAN VMMTranslateUnlocked(UINT64 VirtAddr, UINT64 *PhysAddr)
{
    UINT64 CR4;

    ASM("mov %%cr4, %0" : "=r"(CR4));

    if ((CR4 & VMM_CR4_LA57) != 0)
    {
        return FALSE;
    }

    return VMMTranslateRoot((UINT64 *) (VMMGetCurrentCR3() & VMM_PAGE_MASK), VirtAddr, PhysAddr);
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

STATIC BOOLEAN VMMTableWritableRoot(UINT64 *Root, UINT64 Address)
{
    VOLATILE UINT64 *Table = Root;
    UINT64           Entry;
    UINT64           PhysAddr;

    if (!VMMTranslateRoot(Root, Address, &PhysAddr) || PhysAddr != Address)
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

STATIC BOOLEAN VMMTableWritable(UINT64 Address)
{
    UINT64 CR4;

    ASM("mov %%cr4, %0" : "=r"(CR4));

    if ((CR4 & VMM_CR4_LA57) != 0)
    {
        return FALSE;
    }

    return VMMTableWritableRoot((UINT64 *) (VMMGetCurrentCR3() & VMM_PAGE_MASK), Address);
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

    Table = VMMAllocateAddressSpacePage(PhysMask);

    if (Table == NULL)
    {
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

    if (!VMMCanModifyCurrentRoot() || !VMMKernelMappingsIdle())
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

        ATOMIC_STORE(&Table[Indices[FirstMissingLevel]], (UINT64) NewTables[FirstMissingLevel] | ParentFlags, ATOMIC_RELEASE);

        for (UINT32 Level = FirstMissingLevel; Level < 3; Level++)
        {
            NewTables[Level] = NULL;
        }
    }
    else
    {
        ATOMIC_STORE(&Table[Indices[3]], PhysAddr | VMM_PAGE_PRESENT | Flags, ATOMIC_RELEASE);
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

    if (!VMMCanModifyCurrentRoot() || !VMMKernelMappingsIdle())
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

    ATOMIC_STORE(Entry, 0, ATOMIC_RELEASE);

    if (VMMShootdown(VirtAddr))
    {
        *PhysAddr = EntryValue & VMM_PAGE_MASK;
        Result = TRUE;
    }

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

struct VMM_ADDRESS_SPACE
{
    UINT64            *Root;
    UINT64             Shared[8];
    VMM_ADDRESS_SPACE *Next;
};

STATIC VMM_ADDRESS_SPACE *VMMAddressSpaces;
STATIC VMM_ADDRESS_SPACE *VMMActiveAddressSpaces[SMP_MAX_CPUS];

EXTERN UINT8 __kernel_start[];

STATIC BOOLEAN VMMKernelMappingsIdle(VOID)
{
    return VMMAddressSpaces == NULL;
}

STATIC BOOLEAN VMMAddressSpaceIdentityWritable(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address)
{
    UINT64 *Table = AddressSpace->Root;

    for (UINT32 Level = 0; Level < 4; Level++)
    {
        UINT64 Entry = Table[(Address >> (39 - Level * 9)) & 0x1FF];
        UINT64 Mask;
        UINT64 Offset;

        if ((Entry & (VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE)) != (VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE))
        {
            return FALSE;
        }

        if (Level == 3 || ((Level == 1 || Level == 2) && (Entry & VMM_PAGE_LARGE) != 0))
        {
            Mask = Level == 1 ? VMM_LARGE_1G_MASK : Level == 2 ? VMM_LARGE_2M_MASK : VMM_PAGE_MASK;
            Offset = Address & ((1ULL << (39 - Level * 9)) - 1);
            return (Entry & VMM_PAGE_USER) == 0 && ((Entry & Mask) | Offset) == Address;
        }

        if ((Entry & VMM_PAGE_LARGE) != 0)
        {
            return FALSE;
        }

        Table = (UINT64 *) (Entry & VMM_PAGE_MASK);
    }

    return FALSE;
}

STATIC BOOLEAN VMMAddressSpaceValid(VMM_ADDRESS_SPACE *AddressSpace)
{
    for (VMM_ADDRESS_SPACE *Current = VMMAddressSpaces; Current != NULL; Current = Current->Next)
    {
        if (Current == AddressSpace)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC BOOLEAN VMMAddressSpaceInactive(VMM_ADDRESS_SPACE *AddressSpace)
{
    for (UINT32 Index = 0; Index < SMP_MAX_CPUS; Index++)
    {
        if (VMMActiveAddressSpaces[Index] == AddressSpace)
        {
            return FALSE;
        }
    }

    return TRUE;
}

STATIC BOOLEAN VMMCanModifyFromManagedRoot(VOID)
{
    UINT32             CPUID;
    VMM_ADDRESS_SPACE *Current;

    if (!VMMCanModify())
    {
        return FALSE;
    }
    if (VMMGetCurrentCR3() == SMPGetBootCR3())
    {
        return TRUE;
    }
    CPUID = SMPGetCurrentCPUID();
    if (CPUID >= SMP_MAX_CPUS)
    {
        return FALSE;
    }
    Current = VMMActiveAddressSpaces[CPUID];
    return Current != NULL && VMMAddressSpaceValid(Current) && (VMMGetCurrentCR3() & VMM_PAGE_MASK) == (UINT64) Current->Root;
}
STATIC UINT64 *VMMAllocateAddressSpacePage(UINT64 PhysMask)
{
    UINT64 *Page = KAllocPagesBelow(1, 0x100000000ULL);

    if (Page != NULL && (((UINT64) Page & ~PhysMask) != 0 || !VMMTableWritable((UINT64) Page)))
    {
        KMemFreePages(Page, 1);
        return NULL;
    }

    if (Page != NULL)
    {
        for (VMM_ADDRESS_SPACE *Space = VMMAddressSpaces; Space != NULL; Space = Space->Next)
        {
            if (!VMMAddressSpaceIdentityWritable(Space, (UINT64) Page))
            {
                KMemFreePages(Page, 1);
                return NULL;
            }
        }
    }

    return Page;
}

BOOLEAN VMMCreateAddressSpace(VMM_ADDRESS_SPACE **AddressSpace)
{
    VMM_ADDRESS_SPACE *Space = NULL;
    UINT64            *KernelRoot;
    UINT64             PhysMask;
    UINT64             IRQFlags;
    BOOLEAN            Result = FALSE;

    if (AddressSpace == NULL)
    {
        return FALSE;
    }

    *AddressSpace = NULL;

    if (!VMMWaitLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMCanModifyFromManagedRoot() || !VMMGetPhysicalAddressMask(&PhysMask))
    {
        goto Cleanup;
    }

    KernelRoot = (UINT64 *) (SMPGetBootCR3() & VMM_PAGE_MASK);

    if (!VMMTableWritable((UINT64) KernelRoot))
    {
        goto Cleanup;
    }

    for (UINT32 Index = 0; Index < 512; Index++)
    {
        if ((KernelRoot[Index] & (VMM_PAGE_PRESENT | VMM_PAGE_USER)) == (VMM_PAGE_PRESENT | VMM_PAGE_USER))
        {
            goto Cleanup;
        }
    }

    Space = (VMM_ADDRESS_SPACE *) VMMAllocateAddressSpacePage(PhysMask);

    if (Space == NULL)
    {
        goto Cleanup;
    }

    Space->Root = VMMAllocateAddressSpacePage(PhysMask);

    if (Space->Root == NULL)
    {
        KMemFreePages(Space, 1);
        goto Cleanup;
    }

    for (UINT32 Index = 0; Index < 512; Index++)
    {
        UINT64 Entry = KernelRoot[Index];

        if ((Entry & VMM_PAGE_PRESENT) != 0)
        {
            Space->Root[Index] = Entry;
            Space->Shared[Index / 64] |= 1ULL << (Index % 64);
        }
    }

    Space->Next = VMMAddressSpaces;
    VMMAddressSpaces = Space;
    *AddressSpace = Space;
    Result = TRUE;

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

BOOLEAN VMMDestroyAddressSpace(VMM_ADDRESS_SPACE *AddressSpace)
{
    UINT64              IRQFlags;
    VMM_ADDRESS_SPACE **Link;
    BOOLEAN             Result = FALSE;

    if (!VMMWaitLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMCanModifyFromManagedRoot() || !VMMAddressSpaceValid(AddressSpace) || !VMMAddressSpaceInactive(AddressSpace))
    {
        goto Cleanup;
    }

    for (UINT32 Index = 0; Index < 512; Index++)
    {
        UINT64 Entry = AddressSpace->Root[Index];

        if ((AddressSpace->Shared[Index / 64] & (1ULL << (Index % 64))) == 0 && (Entry & VMM_PAGE_PRESENT) != 0)
        {
            VMMFreeClone((UINT64 *) (Entry & VMM_PAGE_MASK), 1);
        }
    }

    Link = &VMMAddressSpaces;

    while (*Link != AddressSpace)
    {
        Link = &(*Link)->Next;
    }

    *Link = AddressSpace->Next;
    KMemFreePages(AddressSpace->Root, 1);
    KMemFreePages(AddressSpace, 1);
    Result = TRUE;

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

STATIC UINT64 *VMMAddressSpaceLeaf(VMM_ADDRESS_SPACE *AddressSpace, UINT64 VirtAddr)
{
    UINT64 *Table = AddressSpace->Root;

    for (UINT32 Level = 0; Level < 3; Level++)
    {
        UINT64 Entry;

        if (!VMMTableWritable((UINT64) Table))
        {
            return NULL;
        }

        Entry = Table[(VirtAddr >> (39 - Level * 9)) & 0x1FF];

        if ((Entry & VMM_PAGE_PRESENT) == 0 || (Entry & VMM_PAGE_LARGE) != 0)
        {
            return NULL;
        }

        Table = (UINT64 *) (Entry & VMM_PAGE_MASK);
    }

    if (!VMMTableWritable((UINT64) Table))
    {
        return NULL;
    }

    return &Table[(VirtAddr >> 12) & 0x1FF];
}

BOOLEAN VMMTranslateAddressSpace(VMM_ADDRESS_SPACE *AddressSpace, UINT64 VirtAddr, UINT64 *PhysAddr)
{
    UINT64  IRQFlags;
    UINT64 *Table;
    BOOLEAN Result = FALSE;

    if (PhysAddr == NULL || !VMMIsCanonicalAddress(VirtAddr) || !VMMWaitLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMCanModify() || !VMMAddressSpaceValid(AddressSpace))
    {
        goto Cleanup;
    }

    Table = AddressSpace->Root;

    for (UINT32 Level = 0; Level < 4; Level++)
    {
        UINT64 Entry;
        UINT64 Mask;

        if (!VMMTableWritable((UINT64) Table))
        {
            goto Cleanup;
        }

        Entry = Table[(VirtAddr >> (39 - Level * 9)) & 0x1FF];

        if ((Entry & VMM_PAGE_PRESENT) == 0)
        {
            goto Cleanup;
        }

        if (Level == 3 || ((Level == 1 || Level == 2) && (Entry & VMM_PAGE_LARGE) != 0))
        {
            Mask = Level == 1 ? VMM_LARGE_1G_MASK : Level == 2 ? VMM_LARGE_2M_MASK : VMM_PAGE_MASK;
            *PhysAddr = (Entry & Mask) | (VirtAddr & ((1ULL << (39 - Level * 9)) - 1));
            Result = TRUE;
            goto Cleanup;
        }

        if ((Entry & VMM_PAGE_LARGE) != 0)
        {
            goto Cleanup;
        }

        Table = (UINT64 *) (Entry & VMM_PAGE_MASK);
    }

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

STATIC BOOLEAN VMMTableUsesAddress(UINT64 *Table, UINT32 Level, UINT64 Address)
{
    if ((UINT64) Table == Address)
    {
        return TRUE;
    }

    if (Level < 3)
    {
        for (UINT32 Index = 0; Index < 512; Index++)
        {
            UINT64 Entry = Table[Index];

            if ((Entry & VMM_PAGE_PRESENT) != 0 && (Entry & VMM_PAGE_LARGE) == 0 && VMMTableUsesAddress((UINT64 *) (Entry & VMM_PAGE_MASK), Level + 1, Address))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

STATIC BOOLEAN VMMAddressSpaceCanReplace(UINT64 Address)
{
    UINT64 Stack;

    ASM("mov %%rsp, %0" : "=r"(Stack));

    if (Address < 0x400000ULL)
    {
        return FALSE;
    }

    if (Address >= (UINT64) __kernel_start)
    {
        return FALSE;
    }

    if ((Address >> 21) == (Stack >> 21))
    {
        return FALSE;
    }

    if (VMMTableUsesAddress((UINT64 *) (SMPGetBootCR3() & VMM_PAGE_MASK), 0, Address))
    {
        return FALSE;
    }

    for (VMM_ADDRESS_SPACE *Space = VMMAddressSpaces; Space != NULL; Space = Space->Next)
    {
        if ((UINT64) Space == Address)
        {
            return FALSE;
        }

        if (VMMTableUsesAddress(Space->Root, 0, Address))
        {
            return FALSE;
        }
    }

    for (UINT32 Index = 0; Index < SMPGetCPUCount(); Index++)
    {
        CPU_INFO *CPU = SMPGetCPU(Index);

        if (CPU != NULL && CPU->Stack != NULL && Address >= (UINT64) CPU->Stack && Address - (UINT64) CPU->Stack < SMP_AP_STACK_PAGES * VMM_PAGE_SIZE)
        {
            return FALSE;
        }
    }

    return TRUE;
}

STATIC VOID VMMRestrictInheritedTable(UINT64 *Table, UINT32 Level, BOOLEAN Writable, UINT64 NX)
{
    for (UINT32 Index = 0; Index < 512; Index++)
    {
        UINT64 Entry = Table[Index];

        if ((Entry & VMM_PAGE_PRESENT) == 0)
        {
            continue;
        }

        if (!Writable)
        {
            Entry &= ~VMM_PAGE_WRITABLE;
        }

        Entry |= NX;

        if (Level < 3 && (Entry & VMM_PAGE_LARGE) == 0)
        {
            VMMRestrictInheritedTable((UINT64 *) (Entry & VMM_PAGE_MASK), Level + 1, (Entry & VMM_PAGE_WRITABLE) != 0, Entry & VMM_PAGE_NX);
            Entry &= ~VMM_PAGE_NX;
        }

        Table[Index] = Entry & ~VMM_PAGE_USER;
    }
}

STATIC UINT64 *VMMSplitAddressSpacePage(UINT64 Entry, UINT32 Level, UINT64 PhysMask)
{
    UINT64 *Table = VMMAllocateAddressSpacePage(PhysMask);
    UINT64  Address = Entry & (Level == 1 ? VMM_LARGE_1G_MASK : VMM_LARGE_2M_MASK);
    UINT64  Flags = Entry & ~VMM_PAGE_MASK;
    UINT64  Size = Level == 1 ? 0x200000ULL : VMM_PAGE_SIZE;

    if (Table == NULL)
    {
        return NULL;
    }

    if (Level == 1)
    {
        Flags |= Entry & (1ULL << 12);
    }
    else
    {
        Flags &= ~VMM_PAGE_LARGE;

        if ((Entry & (1ULL << 12)) != 0)
        {
            Flags |= VMM_PAGE_LARGE;
        }
    }

    for (UINT32 Index = 0; Index < 512; Index++)
    {
        Table[Index] = (Address + Index * Size) | Flags;
    }

    return Table;
}

BOOLEAN VMMMapAddressSpacePage(VMM_ADDRESS_SPACE *AddressSpace, UINT64 VirtAddr, UINT64 PhysAddr, UINT64 Flags)
{
    UINT64  IRQFlags;
    UINT64  PhysMask;
    UINT64  RootEntry;
    UINT64 *Table;
    UINT64 *Root = NULL;
    UINT32  RootIndex = (VirtAddr >> 39) & 0x1FF;
    BOOLEAN Shared;
    BOOLEAN Result = FALSE;

    if (VirtAddr >= (1ULL << 47) || (VirtAddr >= VMM_SHARED_USER_START && VirtAddr < VMM_SHARED_USER_END))
    {
        return FALSE;
    }

    if ((VirtAddr & 0xFFFULL) != 0 || (PhysAddr & 0xFFFULL) != 0)
    {
        return FALSE;
    }

    if ((Flags & VMM_FLAG_USER) == 0 || (Flags & ~(VMM_FLAG_USER | VMM_FLAG_WRITABLE)) != 0)
    {
        return FALSE;
    }

    if (!VMMGetPhysicalAddressMask(&PhysMask))
    {
        return FALSE;
    }

    if ((PhysAddr & ~PhysMask) != 0)
    {
        return FALSE;
    }

    if (!VMMWaitLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMCanModifyFromManagedRoot())
    {
        goto Cleanup;
    }

    if (!VMMAddressSpaceValid(AddressSpace))
    {
        goto Cleanup;
    }

    if (!VMMAddressSpaceInactive(AddressSpace))
    {
        goto Cleanup;
    }

    RootEntry = AddressSpace->Root[RootIndex];
    Shared = (AddressSpace->Shared[RootIndex / 64] & (1ULL << (RootIndex % 64))) != 0;

    Root = (RootEntry & VMM_PAGE_PRESENT) != 0 ? VMMCloneTable(RootEntry & VMM_PAGE_MASK, 1, PhysMask) : VMMAllocateAddressSpacePage(PhysMask);

    if (Root == NULL)
    {
        goto Cleanup;
    }

    if (Shared)
    {
        VMMRestrictInheritedTable(Root, 1, (RootEntry & VMM_PAGE_WRITABLE) != 0, RootEntry & VMM_PAGE_NX);
    }

    Table = Root;

    for (UINT32 Level = 1; Level < 3; Level++)
    {
        UINT32 Index = (VirtAddr >> (39 - Level * 9)) & 0x1FF;
        UINT64 Entry = Table[Index];

        if ((Entry & VMM_PAGE_PRESENT) == 0 || (Entry & VMM_PAGE_LARGE) != 0)
        {
            UINT64 *Child = (Entry & VMM_PAGE_PRESENT) != 0 ? VMMSplitAddressSpacePage(Entry, Level, PhysMask) : VMMAllocateAddressSpacePage(PhysMask);

            if (Child == NULL)
            {
                goto Cleanup;
            }

            Entry = (UINT64) Child | VMM_PAGE_PRESENT;
        }

        Table[Index] = (Entry & ~VMM_PAGE_NX) | VMM_PAGE_USER | VMM_PAGE_WRITABLE;
        Table = (UINT64 *) (Entry & VMM_PAGE_MASK);
    }

    {
        UINT32 Index = (VirtAddr >> 12) & 0x1FF;
        UINT64 Entry = Table[Index];

        if (VirtAddr < (UINT64) __kernel_start)
        {
            if (!VMMAddressSpaceCanReplace(VirtAddr))
            {
                goto Cleanup;
            }

            if (VMMTableUsesAddress(Root, 1, VirtAddr))
            {
                goto Cleanup;
            }
        }

        if ((Entry & VMM_PAGE_PRESENT) != 0 && ((Entry & (VMM_PAGE_USER | VMM_PAGE_GLOBAL)) != 0 || (Entry & VMM_PAGE_MASK) != VirtAddr || VirtAddr >= (UINT64) __kernel_start))
        {
            goto Cleanup;
        }

        Table[Index] = PhysAddr | VMM_PAGE_PRESENT | Flags;
    }

    AddressSpace->Root[RootIndex] = (UINT64) Root | (RootEntry & (0x18ULL | (1ULL << 5))) | VMM_PAGE_PRESENT | VMM_PAGE_USER | VMM_PAGE_WRITABLE;
    AddressSpace->Shared[RootIndex / 64] &= ~(1ULL << (RootIndex % 64));
    Root = NULL;

    if (!Shared && (RootEntry & VMM_PAGE_PRESENT) != 0)
    {
        VMMFreeClone((UINT64 *) (RootEntry & VMM_PAGE_MASK), 1);
    }

    Result = TRUE;

Cleanup:
    if (Root != NULL)
    {
        VMMFreeClone(Root, 1);
    }

    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

BOOLEAN VMMUnmapAddressSpacePage(VMM_ADDRESS_SPACE *AddressSpace, UINT64 VirtAddr, UINT64 *PhysAddr)
{
    UINT64  IRQFlags;
    UINT64 *Entry;
    UINT32  RootIndex = (VirtAddr >> 39) & 0x1FF;
    BOOLEAN Result = FALSE;

    if (PhysAddr == NULL || VirtAddr >= (1ULL << 47) || (VirtAddr & 0xFFFULL) != 0 || !VMMWaitLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMCanModifyFromManagedRoot() || !VMMAddressSpaceValid(AddressSpace) || !VMMAddressSpaceInactive(AddressSpace) || (AddressSpace->Shared[RootIndex / 64] & (1ULL << (RootIndex % 64))) != 0)
    {
        goto Cleanup;
    }

    Entry = VMMAddressSpaceLeaf(AddressSpace, VirtAddr);

    if (Entry != NULL && (*Entry & (VMM_PAGE_PRESENT | VMM_PAGE_USER)) == (VMM_PAGE_PRESENT | VMM_PAGE_USER))
    {
        *PhysAddr = *Entry & VMM_PAGE_MASK;
        *Entry = 0;
        Result = TRUE;
    }

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

STATIC BOOLEAN VMMSharedRange(UINT64 Address, UINT32 Count)
{
    return Count != 0 && (Address & (VMM_PAGE_SIZE - 1)) == 0 && Address >= VMM_SHARED_USER_START && Address < VMM_SHARED_USER_END && (UINT64) Count <= (VMM_SHARED_USER_END - Address) / VMM_PAGE_SIZE;
}

STATIC BOOLEAN VMMSharedCurrent(VMM_ADDRESS_SPACE *AddressSpace)
{
    UINT32 CPUID = SMPGetCurrentCPUID();

    if (!VMMCanModifyFromManagedRoot() || !VMMAddressSpaceValid(AddressSpace) || CPUID >= SMP_MAX_CPUS || VMMActiveAddressSpaces[CPUID] != AddressSpace || (VMMGetCurrentCR3() & VMM_PAGE_MASK) != (UINT64) AddressSpace->Root)
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < SMP_MAX_CPUS; Index++)
    {
        if (Index != CPUID && VMMActiveAddressSpaces[Index] == AddressSpace)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN VMMMapSharedPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, CONST UINT64 *Pages, UINT32 Count)
{
    UINT64  IRQFlags;
    UINT64  PhysMask;
    UINT64  RootEntry;
    UINT64 *Root = NULL;
    UINT32  RootIndex = (Address >> 39) & 0x1FF;
    BOOLEAN Result = FALSE;

    if (Pages == NULL || !VMMSharedRange(Address, Count) || !VMMGetPhysicalAddressMask(&PhysMask) || !VMMWaitLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMSharedCurrent(AddressSpace) || (AddressSpace->Shared[RootIndex / 64] & (1ULL << (RootIndex % 64))) != 0)
    {
        goto Cleanup;
    }

    RootEntry = AddressSpace->Root[RootIndex];
    Root = (RootEntry & VMM_PAGE_PRESENT) != 0 ? VMMCloneTable(RootEntry & VMM_PAGE_MASK, 1, PhysMask) : VMMAllocateAddressSpacePage(PhysMask);

    if (Root == NULL)
    {
        goto Cleanup;
    }

    for (UINT32 Page = 0; Page < Count; Page++)
    {
        UINT64  Current = Address + (UINT64) Page * VMM_PAGE_SIZE;
        UINT64 *Table = Root;

        if ((Pages[Page] & ~PhysMask) != 0 || !VMMTableWritable(Pages[Page]))
        {
            goto Cleanup;
        }

        for (UINT32 Level = 1; Level < 3; Level++)
        {
            UINT32 Index = (Current >> (39 - Level * 9)) & 0x1FF;
            UINT64 Entry = Table[Index];

            if ((Entry & VMM_PAGE_LARGE) != 0)
            {
                goto Cleanup;
            }

            if ((Entry & VMM_PAGE_PRESENT) == 0)
            {
                UINT64 *Child = VMMAllocateAddressSpacePage(PhysMask);

                if (Child == NULL)
                {
                    goto Cleanup;
                }

                Entry = (UINT64) Child | VMM_PAGE_PRESENT | VMM_PAGE_USER | VMM_PAGE_WRITABLE;
                Table[Index] = Entry;
            }

            Table = (UINT64 *) (Entry & VMM_PAGE_MASK);
        }

        if ((Table[(Current >> 12) & 0x1FF] & VMM_PAGE_PRESENT) != 0)
        {
            goto Cleanup;
        }

        Table[(Current >> 12) & 0x1FF] = Pages[Page] | VMM_PAGE_PRESENT | VMM_PAGE_USER;
    }

    ATOMIC_STORE(&AddressSpace->Root[RootIndex], (UINT64) Root | VMM_PAGE_PRESENT | VMM_PAGE_USER | VMM_PAGE_WRITABLE, ATOMIC_RELEASE);
    ASM("mov %0, %%cr3" : : "r"(VMMGetCurrentCR3()) : "memory");
    Root = NULL;

    if ((RootEntry & VMM_PAGE_PRESENT) != 0)
    {
        VMMFreeClone((UINT64 *) (RootEntry & VMM_PAGE_MASK), 1);
    }

    Result = TRUE;
Cleanup:
    if (Root != NULL)
    {
        VMMFreeClone(Root, 1);
    }

    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

BOOLEAN VMMUnmapSharedPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, CONST UINT64 *Pages, UINT32 Count)
{
    UINT64  IRQFlags;
    UINT32  RootIndex = (Address >> 39) & 0x1FF;
    BOOLEAN Result = FALSE;

    if (Pages == NULL || !VMMSharedRange(Address, Count) || !VMMWaitLock(&IRQFlags))
    {
        return FALSE;
    }

    if (!VMMSharedCurrent(AddressSpace) || (AddressSpace->Shared[RootIndex / 64] & (1ULL << (RootIndex % 64))) != 0)
    {
        goto Cleanup;
    }

    for (UINT32 Page = 0; Page < Count; Page++)
    {
        UINT64 *Entry = VMMAddressSpaceLeaf(AddressSpace, Address + (UINT64) Page * VMM_PAGE_SIZE);

        if (Entry == NULL || *Entry != (Pages[Page] | VMM_PAGE_PRESENT | VMM_PAGE_USER | (*Entry & ((1ULL << 5) | (1ULL << 6)))))
        {
            goto Cleanup;
        }
    }

    for (UINT32 Page = 0; Page < Count; Page++)
    {
        UINT64 *Entry = VMMAddressSpaceLeaf(AddressSpace, Address + (UINT64) Page * VMM_PAGE_SIZE);

        ATOMIC_STORE(Entry, 0, ATOMIC_RELEASE);
    }

    ASM("mov %0, %%cr3" : : "r"(VMMGetCurrentCR3()) : "memory");
    Result = TRUE;
Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

BOOLEAN VMMSwitchAddressSpace(VMM_ADDRESS_SPACE *AddressSpace)
{
    UINT32  CPUID = SMPGetCurrentCPUID();
    UINT64  CR4;
    UINT64  CurrentCR3;
    UINT64  TargetCR3;
    UINT64  IRQFlags;
    BOOLEAN Result = FALSE;

    ASM("pushfq; popq %0; cli" : "=r"(IRQFlags) : : "memory");

    while (ATOMIC_EXCHANGE(&VMMMapLock.Locked, 1, ATOMIC_ACQUIRE) != 0)
    {
        VMMHandleShootdown();
        ASM("pause" ::: "memory");
    }

    ASM("mov %%cr4, %0" : "=r"(CR4));

    if (!VMMInitialized || !SMPIsReady() || (VMMFailed && AddressSpace != NULL) || (CR4 & (VMM_CR4_LA57 | (1ULL << 17))) != 0 || CPUID >= SMP_MAX_CPUS)
    {
        goto Cleanup;
    }

    CurrentCR3 = VMMGetCurrentCR3();

    if (AddressSpace == NULL)
    {
        TargetCR3 = SMPGetBootCR3();
    }
    else
    {
        if (!VMMAddressSpaceValid(AddressSpace) || AddressSpace->Root == NULL || ((UINT64) AddressSpace->Root & 0xFFFULL) != 0 || !VMMTableWritable((UINT64) AddressSpace->Root))
        {
            goto Cleanup;
        }

        TargetCR3 = (UINT64) AddressSpace->Root | (CurrentCR3 & 0x18ULL);
    }

    if (TargetCR3 != CurrentCR3)
    {
        ASM("mov %0, %%cr3" : : "r"(TargetCR3) : "memory");
    }

    VMMActiveAddressSpaces[CPUID] = AddressSpace;
    Result = TRUE;

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

STATIC BOOLEAN VMMUserPagePhysical(VMM_ADDRESS_SPACE *AddressSpace, UINT64 *KernelRoot, UINT64 VirtAddr, BOOLEAN Writable, UINT64 *PhysAddr)
{
    UINT64 *Table;
    UINT64  Entry;
    UINT64  Mask;
    UINT64  Offset;

    if (AddressSpace == NULL || PhysAddr == NULL || VirtAddr >= (1ULL << 47))
    {
        return FALSE;
    }

    Table = AddressSpace->Root;

    for (UINT32 Level = 0; Level < 4; Level++)
    {
        if (!VMMTableWritableRoot(KernelRoot, (UINT64) Table))
        {
            return FALSE;
        }

        Entry = Table[(VirtAddr >> (39 - Level * 9)) & 0x1FF];

        if ((Entry & (VMM_PAGE_PRESENT | VMM_PAGE_USER)) != (VMM_PAGE_PRESENT | VMM_PAGE_USER))
        {
            return FALSE;
        }

        if (Writable && (Entry & VMM_PAGE_WRITABLE) == 0)
        {
            return FALSE;
        }

        if (Level == 3 || ((Level == 1 || Level == 2) && (Entry & VMM_PAGE_LARGE) != 0))
        {
            Mask = Level == 1 ? VMM_LARGE_1G_MASK : Level == 2 ? VMM_LARGE_2M_MASK : VMM_PAGE_MASK;
            Offset = VirtAddr & ((1ULL << (39 - Level * 9)) - 1);

            *PhysAddr = (Entry & Mask) | Offset;
            return TRUE;
        }

        if ((Entry & VMM_PAGE_LARGE) != 0)
        {
            return FALSE;
        }

        Table = (UINT64 *) (Entry & VMM_PAGE_MASK);
    }

    return FALSE;
}

STATIC BOOLEAN VMMCopyUser(VMM_ADDRESS_SPACE *AddressSpace, UINT64 UserAddress, VOID *KernelBuffer, UINT64 Size, BOOLEAN ToUser)
{
    UINT64 *KernelRoot;
    UINT64  IRQFlags;
    UINT64  Current;
    UINT64  Physical;
    UINT64  Identity;
    UINT64  Chunk;
    UINT64  Offset;
    UINT64  Checked;
    BOOLEAN Result = FALSE;

    if (AddressSpace == NULL || KernelBuffer == NULL || Size == 0)
    {
        return FALSE;
    }

    if (UserAddress >= (1ULL << 47) || Size > (1ULL << 47) - UserAddress)
    {
        return FALSE;
    }

    ASM("pushfq; popq %0; cli" : "=r"(IRQFlags) : : "memory");

    while (ATOMIC_EXCHANGE(&VMMMapLock.Locked, 1, ATOMIC_ACQUIRE) != 0)
    {
        VMMHandleShootdown();
        ASM("pause" ::: "memory");
    }

    if (!VMMCanModify() || !VMMAddressSpaceValid(AddressSpace))
    {
        goto Cleanup;
    }

    KernelRoot = (UINT64 *) (VMMGetCurrentCR3() & VMM_PAGE_MASK);
    Checked = 0;

    while (Checked < Size)
    {
        Current = UserAddress + Checked;
        Chunk = VMM_PAGE_SIZE - (Current & (VMM_PAGE_SIZE - 1));

        if (Chunk > Size - Checked)
        {
            Chunk = Size - Checked;
        }

        if (!VMMUserPagePhysical(AddressSpace, KernelRoot, Current, ToUser, &Physical))
        {
            goto Cleanup;
        }

        if (!VMMTranslateRoot(KernelRoot, Physical, &Identity) || Identity != Physical)
        {
            goto Cleanup;
        }

        if (ToUser && !VMMTableWritableRoot(KernelRoot, Physical))
        {
            goto Cleanup;
        }

        Checked += Chunk;
    }

    Offset = 0;

    while (Offset < Size)
    {
        Current = UserAddress + Offset;
        Chunk = VMM_PAGE_SIZE - (Current & (VMM_PAGE_SIZE - 1));

        if (Chunk > Size - Offset)
        {
            Chunk = Size - Offset;
        }

        if (!VMMUserPagePhysical(AddressSpace, KernelRoot, Current, ToUser, &Physical))
        {
            goto Cleanup;
        }

        if (ToUser)
        {
            KMemCpy((VOID *) Physical, (CONST UINT8 *) KernelBuffer + Offset, Chunk);
        }
        else
        {
            KMemCpy((UINT8 *) KernelBuffer + Offset, (CONST VOID *) Physical, Chunk);
        }

        Offset += Chunk;
    }

    Result = TRUE;

Cleanup:
    SpinLockReleaseIRQRestore(&VMMMapLock, IRQFlags);
    return Result;
}

BOOLEAN VMMCopyFromUser(VMM_ADDRESS_SPACE *AddressSpace, VOID *Destination, UINT64 Source, UINT64 Size)
{
    return VMMCopyUser(AddressSpace, Source, Destination, Size, FALSE);
}

BOOLEAN VMMCopyToUser(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Destination, CONST VOID *Source, UINT64 Size)
{
    return VMMCopyUser(AddressSpace, Destination, (VOID *) Source, Size, TRUE);
}
