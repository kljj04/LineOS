// LOSLibC/Source/Asset/Asset.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Asset.h>
#include <syscall.h>
#include <errno.h>

INT64 LOSAssetPublish(CONST char *Name, CONST VOID *Data, UINT64 Size)
{
    return (INT64) LineOSSyscallResult(LineOSSyscall3(SYSCALL_ASSET_PUBLISH, (UINT64) Name, (UINT64) Data, Size));
}

INT64 LOSAssetAcquire(CONST char *Name, LOS_ASSET *Asset)
{
    return (INT64) LineOSSyscallResult(LineOSSyscall2(SYSCALL_ASSET_ACQUIRE, (UINT64) Name, (UINT64) Asset));
}

INT64 LOSAssetRelease(LOS_ASSET *Asset)
{
    INT64 Result;

    if (Asset == NULL || Asset->Handle == 0)
    {
        errno = EINVAL;

        return -1;
    }

    Result = (INT64) LineOSSyscallResult(LineOSSyscall1(SYSCALL_ASSET_RELEASE, Asset->Handle));

    if (Result == 0)
    {
        Asset->Data = NULL;
        Asset->Size = 0;
        Asset->Handle = 0;
    }

    return Result;
}
