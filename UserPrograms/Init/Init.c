// UserPrograms/Init/Init.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <Asset.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <spawn.h>
#include <stdio.h>

#define INIT_MAX_FONT_SIZE (8U * 1024U * 1024U)
#define INIT_READ_CHUNK    (1024U * 1024U)

STATIC UINT8 FontData[INIT_MAX_FONT_SIZE];

STATIC BOOLEAN InitReadFont(CONST char *Path, UINT8 *Buffer, UINTN Capacity, UINTN *FileSize)
{
    INT32   FD;
    ssize_t ReadSize;
    UINTN   Total;
    UINT8   ExtraByte;
    BOOLEAN Result;

    if (Path == NULL || Buffer == NULL || FileSize == NULL || Capacity == 0)
    {
        return FALSE;
    }

    FD = open(Path, O_RDONLY);

    if (FD < 0)
    {
        printf("Init: cannot open font: %s\n", Path);
        return FALSE;
    }

    Total = 0;
    Result = FALSE;

    while (Total < Capacity)
    {
        UINTN Remaining;

        Remaining = Capacity - Total;

        if (Remaining > INIT_READ_CHUNK)
        {
            Remaining = INIT_READ_CHUNK;
        }

        ReadSize = read(FD, Buffer + Total, Remaining);

        if (ReadSize < 0)
        {
            printf("Init: font read failed: %s\n", Path);
            goto Cleanup;
        }

        if (ReadSize == 0)
        {
            break;
        }

        if ((UINTN) ReadSize > Remaining)
        {
            goto Cleanup;
        }

        Total += (UINTN) ReadSize;
    }

    if (Total == Capacity)
    {
        ReadSize = read(FD, &ExtraByte, 1);

        if (ReadSize != 0)
        {
            printf("Init: font exceeds buffer capacity: %s\n", Path);
            goto Cleanup;
        }
    }

    if (Total == 0)
    {
        printf("Init: empty font file: %s\n", Path);
        goto Cleanup;
    }

    *FileSize = Total;
    Result = TRUE;

Cleanup:
    if (close(FD) != 0)
    {
        Result = FALSE;
    }

    return Result;
}

STATIC BOOLEAN InitPublishFont(CONST char *Path, CONST char *Name)
{
    UINTN Size = 0;

    if (!InitReadFont(Path, FontData, sizeof(FontData), &Size) || LOSAssetPublish(Name, FontData, Size) != 0)
    {
        printf("Init: asset publication failed: %s\n", Name);
        return FALSE;
    }

    printf("Init: published %s (%u bytes)\n", Name, (UINT32) Size);
    return TRUE;
}

INT32 _Start(VOID)
{
    if (open("/dev/lsh1", O_RDONLY) != 0 || open("/dev/lsh1", O_WRONLY) != 1 || open("/dev/lsh1", O_WRONLY) != 2)
    {
        exit(1);
    }

    printf("Init: loading shared assets\n");

    if (!InitPublishFont("/sys/fonts/PTDSB.ttf", "Fonts/Pretendard") || !InitPublishFont("/sys/fonts/JBMNFSB.ttf", "Fonts/JetBrainsMono"))
    {
        exit(1);
    }

    INT32 Master = open("/dev/lsh1/master", O_RDWR);

    if (Master != 3 || spawn("/sbin/Terminal.lix") < 0)
    {
        puts("Init: Terminal spawn failed");
        exit(1);
    }
    close(Master);

    if (spawn("/bin/Shell.lix") < 0)
    {
        printf("Init: Shell spawn failed\n");
        exit(1);
    }

    exit(0);
}
