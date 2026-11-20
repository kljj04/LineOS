// Scripts/Validation/ProgramConfig/Runtime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Asset.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

STATIC UINT8 Stored[256];
STATIC UINT64 Length;
STATIC UINT32 Published;
STATIC UINT32 Acquired;
STATIC UINT32 Released;

INT64 LOSAssetPublish(CONST char *Name, CONST VOID *Data, UINT64 Size)
{
    if (getenv("DENY_PUBLISH") != NULL)
    {
        errno = EPERM;
        return -1;
    }
    if (strcmp(Name, "Validation/Bytes") != 0 || Size > sizeof(Stored))
    {
        abort();
    }
    memcpy(Stored, Data, Size);
    Length = Size;
    Published++;
    return 0;
}

INT64 LOSAssetAcquire(CONST char *Name, LOS_ASSET *Asset)
{
    if (getenv("MISSING_ASSET") != NULL)
    {
        errno = ENOENT;
        return -1;
    }
    if (strcmp(Name, "Validation/Bytes") != 0 || Published != 1)
    {
        abort();
    }
    Asset->Data = Stored;
    Asset->Size = Length;
    Asset->Handle = 1;
    Acquired++;
    return 0;
}

INT64 LOSAssetRelease(LOS_ASSET *Asset)
{
    if (Asset->Handle != 1)
    {
        abort();
    }
    memset(Asset, 0, sizeof(*Asset));
    Released++;
    return 0;
}

INT32 main(VOID)
{
    if (Published != 1 || Acquired != 1 || Released != 1 || Length != 8 || memcmp(Stored, "payload!", 8) != 0)
    {
        return 1;
    }
    puts("main reached after publication and dependency check");
    return 0;
}
