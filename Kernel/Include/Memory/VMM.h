// Kernel/Include/Memory/VMM.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define VMM_FLAG_WRITABLE (1ULL << 1)
#define VMM_FLAG_USER     (1ULL << 2)

typedef struct VMM_ADDRESS_SPACE VMM_ADDRESS_SPACE;

BOOLEAN VMMInit(VOID);
UINT64  VMMGetCurrentCR3(VOID);
BOOLEAN VMMTranslate(UINT64 VirtAddr, UINT64 *PhysAddr);
BOOLEAN VMMMapPage(UINT64 VirtAddr, UINT64 PhysAddr, UINT64 Flags);
BOOLEAN VMMUnmapPage(UINT64 VirtAddr, UINT64 *PhysAddr);
VOID    VMMHandleShootdown(VOID);
BOOLEAN VMMCreateAddressSpace(VMM_ADDRESS_SPACE **AddressSpace);
BOOLEAN VMMDestroyAddressSpace(VMM_ADDRESS_SPACE *AddressSpace);
BOOLEAN VMMTranslateAddressSpace(VMM_ADDRESS_SPACE *AddressSpace, UINT64 VirtAddr, UINT64 *PhysAddr);
BOOLEAN VMMMapAddressSpacePage(VMM_ADDRESS_SPACE *AddressSpace, UINT64 VirtAddr, UINT64 PhysAddr, UINT64 Flags);
BOOLEAN VMMUnmapAddressSpacePage(VMM_ADDRESS_SPACE *AddressSpace, UINT64 VirtAddr, UINT64 *PhysAddr);
BOOLEAN VMMSwitchAddressSpace(VMM_ADDRESS_SPACE *AddressSpace);
BOOLEAN VMMCopyFromUser(VMM_ADDRESS_SPACE *AddressSpace, VOID *Destination, UINT64 Source, UINT64 Size);
BOOLEAN VMMReadUserWord(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, UINT32 *Word);
BOOLEAN VMMWriteUserWord(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, UINT32 Word);
BOOLEAN VMMCompareExchangeUser32(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, UINT32 *Expected, UINT32 Desired);
BOOLEAN VMMCopyToUser(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Destination, CONST VOID *Source, UINT64 Size);

#define VMM_SHARED_USER_START 0x500000000000ULL
#define VMM_SHARED_USER_END   (VMM_SHARED_USER_START + (1ULL << 30))

BOOLEAN VMMMapSharedPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, CONST UINT64 *Pages, UINT32 Count);
BOOLEAN VMMUnmapSharedPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, CONST UINT64 *Pages, UINT32 Count);

#define VMM_FLAG_NO_ACCESS  (1ULL << 9)
#define VMM_FLAG_NO_EXECUTE (1ULL << 63)
#define VMM_MMAP_START      0x600000000000ULL
#define VMM_MMAP_END        0x600100000000ULL

typedef struct
{
    UINT64 Address;
    UINT64 Physical;
    UINT64 Flags;
} VMM_USER_PAGE;

BOOLEAN VMMMapUserPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, CONST UINT64 *Pages, UINT32 Count, UINT64 Flags);
BOOLEAN VMMUnmapUserPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, CONST UINT64 *Pages, UINT32 Count);
BOOLEAN VMMListUserPages(VMM_ADDRESS_SPACE *AddressSpace, VMM_USER_PAGE *Pages, UINT64 Capacity, UINT64 *Count);

BOOLEAN VMMProtectUserPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, UINT32 Count, UINT64 Flags);

BOOLEAN VMMReplaceUserPages(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Address, CONST UINT64 *Pages, CONST UINT64 *Expected, UINT32 Count, UINT64 Flags);
