// UserPrograms/Shell/Shell.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeEngine.h>
#include <Display.h>
#include <OutputStream.h>
#include <Asset.h>
#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHELL_MAX_WIDTH          2560U
#define SHELL_MAX_HEIGHT         1440U
#define SHELL_ALLOCATOR_CAPACITY (8U * 1024U * 1024U)
#define SHELL_FONT_HEIGHT        24U
#define SHELL_LINE_HEIGHT        32U
#define SHELL_MARGIN             8U
#define SHELL_BACKGROUND         0x00101820U
#define SHELL_FOREGROUND         0x00FFFFFFU
#define SHELL_YIELD              6ULL

STATIC UINT32 FrameBuffer[SHELL_MAX_WIDTH * SHELL_MAX_HEIGHT];
STATIC UINT8  AllocatorBuffer[SHELL_ALLOCATOR_CAPACITY];
STATIC UINTN  AllocatorUsed;

STATIC VOID *ShellAllocate(UINTN Size, VOID *Context)
{
    UINTN Aligned;
    VOID *Result;

    (VOID) Context;

    if (Size > SHELL_ALLOCATOR_CAPACITY - 15U)
    {
        return NULL;
    }

    Aligned = (Size + 15U) & ~(UINTN) 15U;

    if (AllocatorUsed > SHELL_ALLOCATOR_CAPACITY - Aligned)
    {
        return NULL;
    }

    Result = &AllocatorBuffer[AllocatorUsed];
    AllocatorUsed += Aligned;

    return Result;
}

STATIC VOID ShellFree(VOID *Pointer, VOID *Context)
{
    (VOID) Pointer;
    (VOID) Context;
}

STATIC VOID ShellNewLine(DISPLAY_INFO *Info, UINT32 *X, UINT32 *Baseline)
{
    *X = SHELL_MARGIN;
    *Baseline += SHELL_LINE_HEIGHT;

    if (*Baseline + SHELL_LINE_HEIGHT > Info->Height)
    {
        UINTN Rows = Info->Height - SHELL_LINE_HEIGHT;

        memmove(FrameBuffer, FrameBuffer + SHELL_LINE_HEIGHT * Info->Width, Rows * Info->Width * sizeof(UINT32));

        for (UINTN Index = Rows * Info->Width; Index < (UINTN) Info->Height * Info->Width; Index++)
        {
            FrameBuffer[Index] = SHELL_BACKGROUND;
        }

        *Baseline -= SHELL_LINE_HEIGHT;
    }
}

INT32 _Start(VOID)
{
    DISPLAY_INFO      Info;
    LOS_ASSET         FontAsset = {0};
    TRUE_TYPE_FONT    Font = {0};
    TRUE_TYPE_SURFACE Surface;
    TTF_ALLOCATOR     Allocator = {ShellAllocate, ShellFree, NULL};
    UINT32            X = SHELL_MARGIN;
    UINT32            Baseline = SHELL_MARGIN + SHELL_FONT_HEIGHT;
    CHAR8             Buffer[256];

    if (DisplayGetInfo(&Info) < 0 || Info.Width < SHELL_MARGIN + SHELL_FONT_HEIGHT || Info.Height < SHELL_LINE_HEIGHT * 2U || Info.Width > SHELL_MAX_WIDTH || Info.Height > SHELL_MAX_HEIGHT || Info.Format != DISPLAY_FORMAT_BGRA8888 || LOSAssetAcquire("Fonts/JetBrainsMono", &FontAsset) != 0)
    {
        puts("Shell: initialization failed");
        exit(1);
    }

    Surface.FrameBuffer = FrameBuffer;
    Surface.FrameBufferWidth = Info.Width;
    Surface.FrameBufferHeight = Info.Height;

    if (!TrueTypeInit(&Allocator, &Surface) || !TrueTypeRegisterFont(&Font, FontAsset.Data))
    {
        puts("Shell: font initialization failed");
        exit(1);
    }

    for (UINTN Index = 0; Index < (UINTN) Info.Width * Info.Height; Index++)
    {
        FrameBuffer[Index] = SHELL_BACKGROUND;
    }

    if (DisplayPresent(FrameBuffer, (UINT64) Info.Width * Info.Height * sizeof(UINT32)) < 0)
    {
        exit(1);
    }

    puts("Shell started");

    for (;;)
    {
        ssize_t Count = LOSOutputStreamRead(Buffer, sizeof(Buffer));

        if (Count <= 0)
        {
            LineOSSyscall0(SHELL_YIELD);
            continue;
        }

        for (ssize_t Index = 0; Index < Count; Index++)
        {
            UINT8 Character = (UINT8) Buffer[Index];

            if (Character == '\n')
            {
                ShellNewLine(&Info, &X, &Baseline);
            }
            else if (Character == '\r')
            {
                X = SHELL_MARGIN;
            }
            else if (Character >= 32U && Character < 127U)
            {
                if (X + SHELL_FONT_HEIGHT >= Info.Width)
                {
                    ShellNewLine(&Info, &X, &Baseline);
                }

                AllocatorUsed = 0;
                X = DrawTrueTypeCodepoint(&Font, Character, X, Baseline, SHELL_FOREGROUND, SHELL_FONT_HEIGHT);
            }
        }

        if (DisplayPresent(FrameBuffer, (UINT64) Info.Width * Info.Height * sizeof(UINT32)) < 0)
        {
            exit(1);
        }
    }
}
