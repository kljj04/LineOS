// UserPrograms/Init/Init.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeEngine.h>
#include <Display.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>

#define INIT_MAX_WIDTH          2560U
#define INIT_MAX_HEIGHT         1440U
#define INIT_MAX_FONT_SIZE      (8U * 1024U * 1024U)
#define INIT_ALLOCATOR_SIZE     (8U * 1024U * 1024U)
#define INIT_READ_CHUNK         4096U
#define INIT_ALLOCATOR_ALIGN    16U

#define INIT_BACKGROUND_COLOR   0x00101820U
#define INIT_TEXT_COLOR         0x00FFFFFFU
#define INIT_ACCENT_COLOR       0x0000C0FFU

#define INIT_FONT_PRETENDARD    "/sys/fonts/PTDSB.ttf"
#define INIT_FONT_JETBRAINS     "/sys/fonts/JBMNFSB.ttf"

STATIC UINT32 FrameBuffer[INIT_MAX_WIDTH * INIT_MAX_HEIGHT];
STATIC UINT8 PretendardData[INIT_MAX_FONT_SIZE];
STATIC UINT8 JetBrainsMonoData[INIT_MAX_FONT_SIZE];
STATIC UINT8 AllocatorBuffer[INIT_ALLOCATOR_SIZE];

STATIC UINTN AllocatorUsed;

STATIC VOID *InitAllocate(UINTN Size, VOID *Context)
{
    UINTN AlignedSize;
    VOID *Result;

    (VOID) Context;

    if (Size > sizeof(AllocatorBuffer) - INIT_ALLOCATOR_ALIGN)
    {
        return NULL;
    }

    if (Size > (UINTN) -1 - (INIT_ALLOCATOR_ALIGN - 1U))
    {
        return NULL;
    }

    AlignedSize = (Size + INIT_ALLOCATOR_ALIGN - 1U)
        & ~(INIT_ALLOCATOR_ALIGN - 1U);

    if (AllocatorUsed > sizeof(AllocatorBuffer)
        || AlignedSize > sizeof(AllocatorBuffer) - AllocatorUsed)
    {
        return NULL;
    }

    Result = &AllocatorBuffer[AllocatorUsed];
    AllocatorUsed += AlignedSize;

    return Result;
}

STATIC VOID InitFree(VOID *Pointer, VOID *Context)
{
    (VOID) Pointer;
    (VOID) Context;
}

STATIC BOOLEAN InitReadFont(CONST char *Path, UINT8 *Buffer, UINTN Capacity, UINTN *FileSize)
{
    INT32 FD;
    ssize_t ReadSize;
    UINTN Total;
    UINT8 ExtraByte;
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

INT32 _Start(VOID)
{
    DISPLAY_INFO DisplayInfo;
    TRUE_TYPE_SURFACE Surface;
    TTF_ALLOCATOR Allocator;
    TRUE_TYPE_FONT Pretendard;
    TRUE_TYPE_FONT JetBrainsMono;

    UINTN PretendardSize;
    UINTN JetBrainsMonoSize;

    AllocatorUsed = 0;
    PretendardSize = 0;
    JetBrainsMonoSize = 0;

    memset(&DisplayInfo, 0, sizeof(DisplayInfo));
    memset(&Surface, 0, sizeof(Surface));
    memset(&Allocator, 0, sizeof(Allocator));
    memset(&Pretendard, 0, sizeof(Pretendard));
    memset(&JetBrainsMono, 0, sizeof(JetBrainsMono));

    printf("Init: starting userspace TrueType test\n");

    if (DisplayGetInfo(&DisplayInfo) < 0)
    {
        printf("Init: DisplayGetInfo failed\n");
        exit(1);
    }

    if (DisplayInfo.Width == 0
        || DisplayInfo.Height == 0
        || DisplayInfo.Width > INIT_MAX_WIDTH
        || DisplayInfo.Height > INIT_MAX_HEIGHT)
    {
        printf("Init: unsupported display dimensions\n");
        exit(1);
    }

    if (DisplayInfo.Format != DISPLAY_FORMAT_BGRA8888)
    {
        printf("Init: unsupported display format\n");
        exit(1);
    }

    if (DisplayInfo.Pitch < DisplayInfo.Width * sizeof(UINT32))
    {
        printf("Init: invalid display pitch\n");
        exit(1);
    }

    printf("Init: display %u x %u\n",
        DisplayInfo.Width, DisplayInfo.Height);

    if (!InitReadFont(
        INIT_FONT_PRETENDARD,
        PretendardData,
        sizeof(PretendardData),
        &PretendardSize))
    {
        exit(1);
    }

    if (!InitReadFont(
        INIT_FONT_JETBRAINS,
        JetBrainsMonoData,
        sizeof(JetBrainsMonoData),
        &JetBrainsMonoSize))
    {
        exit(1);
    }

    printf("Init: Pretendard loaded (%u bytes)\n",
        (UINT32) PretendardSize);

    printf("Init: JetBrains Mono loaded (%u bytes)\n",
        (UINT32) JetBrainsMonoSize);

    Allocator.Allocate = InitAllocate;
    Allocator.Free = InitFree;
    Allocator.Context = NULL;

    Surface.FrameBuffer = FrameBuffer;
    Surface.FrameBufferWidth = DisplayInfo.Width;
    Surface.FrameBufferHeight = DisplayInfo.Height;

    if (!TrueTypeInit(&Allocator, &Surface))
    {
        printf("Init: TrueTypeInit failed\n");
        exit(1);
    }

    if (!TrueTypeRegisterFont(&Pretendard, PretendardData))
    {
        printf("Init: Pretendard registration failed\n");
        exit(1);
    }

    if (!TrueTypeRegisterFont(&JetBrainsMono, JetBrainsMonoData))
    {
        printf("Init: JetBrains Mono registration failed\n");
        exit(1);
    }

    if (spawn("/bin/Shell.lix") < 0)
    {
        printf("Init: Shell spawn failed\n");
        exit(1);
    }

    exit(0);
}
