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
BOOLEAN VMMCopyToUser(VMM_ADDRESS_SPACE *AddressSpace, UINT64 Destination, CONST VOID *Source, UINT64 Size);