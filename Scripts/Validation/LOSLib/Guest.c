// Scripts/Validation/LOSLib/Guest.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Userspace/TypeInfo.h>
#include <Asset.h>
#include <IPC.h>
#include <LOS.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

STATIC __thread INT32 TLSValue = 37;

#define CHECK(Condition) \
    if (!(Condition)) \
    { \
        Result = __LINE__; \
        goto Done; \
    }

INT32 main(VOID)
{
    INT64 Result = 0;
    CHECK(TLSValue == 37);
    CHECK(LOSRemove("/loslib-not-present") == -1 && errno == ENOENT);
    VOID *Memory = malloc(4096);
    CHECK(Memory != NULL);
    memset(Memory, 42, 4096);
    CHECK(((UINT8 *) Memory)[4095] == 42);
    free(Memory);
    INT32 FD = open("/sbin/Init.lix", O_RDONLY);
    CHECK(FD >= 0);
    UINT8 Header[4];
    CHECK(pread(FD, Header, sizeof(Header), 0) == sizeof(Header));
    CHECK(memcmp(Header, "\177ELF", sizeof(Header)) == 0);
    VOID *Mapped = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, FD, 0);
    CHECK(Mapped != MAP_FAILED);
    CHECK(close(FD) == 0);
    CHECK(memcmp(Mapped, Header, sizeof(Header)) == 0);
    CHECK(munmap(Mapped, 4096) == 0);
    CONST char Bytes[] = "LOSLib with musl";
    CHECK(LOSAssetPublish("Validation/LOSLib", Bytes, sizeof(Bytes)) == 0);
    LOS_ASSET Asset = {0};
    CHECK(LOSAssetAcquire("Validation/LOSLib", &Asset) == 0);
    CHECK(Asset.Size == sizeof(Bytes) && memcmp(Asset.Data, Bytes, sizeof(Bytes)) == 0);
    CHECK(LOSAssetRelease(&Asset) == 0);
    INT64 Endpoint = LOSIPCCreate(NULL);
    CHECK(Endpoint >= 0);
    CHECK(LOSIPCClose((UINT64) Endpoint) == 0);
Done:
    FD = open(Result == 0 ? "/loslib-musl-PASS" : "/loslib-musl-FAIL", O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (FD >= 0)
    {
        write(FD, &Result, sizeof(Result));
        close(FD);
    }
    for (;;)
    {
        sched_yield();
    }
}
