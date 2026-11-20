// Kernel/Include/Syscall/Asset/SyscallAsset.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

INT64 SyscallAssetPublish(SYSCALL_CONTEXT *Context);
INT64 SyscallAssetAcquire(SYSCALL_CONTEXT *Context);
INT64 SyscallAssetRelease(SYSCALL_CONTEXT *Context);
