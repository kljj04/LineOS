// Common/Syscall/Asset/Asset.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#ifdef LINEOS_KERNEL_BUILD
#include <LineOS/TypeInfo.h>
#else
#include <LOSLibC/TypeInfo.h>
#endif

#define LOS_ASSET_NAME_LIMIT    64U
#define LOS_ASSET_MAX_SIZE      (16ULL * 1024 * 1024)
#define LOS_ASSET_MAPPING_LIMIT 8U

typedef struct
{
    CONST VOID *Data;
    UINT64      Size;
    UINT64      Handle;
} LOS_ASSET;
