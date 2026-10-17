// kernel/include/memory/VMM.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define VMM_FLAG_WRITABLE (1ULL << 1)
#define VMM_FLAG_USER     (1ULL << 2)

UINT64  VMMGetCurrentCR3(VOID);
BOOLEAN VMMTranslate(UINT64 VirtAddr, UINT64 *PhysAddr);
BOOLEAN VMMMapPage(UINT64 VirtAddr, UINT64 PhysAddr, UINT64 Flags);
