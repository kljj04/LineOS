// Kernel/Include/Syscall/SyscallAsset.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

INT64 SyscallAssetPublish(UINT64 Name, UINT64 Data, UINT64 Size);
INT64 SyscallAssetAcquire(UINT64 Name, UINT64 UserAsset);
INT64 SyscallAssetRelease(UINT64 Handle);
