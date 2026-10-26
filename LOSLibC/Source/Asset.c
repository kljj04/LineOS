// LOSLibC/Source/Asset.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Asset.h>
#include <syscall.h>

INT64 LOSAssetPublish(CONST char *Name, CONST VOID *Data, UINT64 Size)
{
    return (INT64) LineOSSyscall3(12, (UINT64) Name, (UINT64) Data, Size);
}

INT64 LOSAssetAcquire(CONST char *Name, LOS_ASSET *Asset)
{
    return (INT64) LineOSSyscall2(13, (UINT64) Name, (UINT64) Asset);
}

INT64 LOSAssetRelease(LOS_ASSET *Asset)
{
    INT64 Result;

    if (Asset == NULL || Asset->Handle == 0)
    {
        return -1;
    }

    Result = (INT64) LineOSSyscall1(14, Asset->Handle);

    if (Result == 0)
    {
        Asset->Data = NULL;
        Asset->Size = 0;
        Asset->Handle = 0;
    }

    return Result;
}
