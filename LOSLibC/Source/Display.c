// LOSLibC/Source/Display.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Display.h>
#include <syscall.h>
#include <errno.h>

INT64 DisplayGetInfo(DISPLAY_INFO *Info)
{
    if (Info == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    return (INT64) LineOSSyscallResult(LineOSSyscall1(SYSCALL_DISPLAY_INFO, (UINT64) Info));
}

INT64 DisplayPresent(CONST VOID *Buffer, UINT64 BufferSize)
{
    if (Buffer == NULL || BufferSize == 0)
    {
        errno = EINVAL;
        return -1;
    }

    return (INT64) LineOSSyscallResult(LineOSSyscall2(SYSCALL_DISPLAY_PRESENT, (UINT64) Buffer, BufferSize));
}

INT64 DisplayPresentRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, CONST VOID *Buffer)
{
    DISPLAY_RECT Rect;

    if (Buffer == NULL || Width == 0 || Height == 0)
    {
        errno = EINVAL;
        return -1;
    }

    Rect.X = X;
    Rect.Y = Y;
    Rect.Width = Width;
    Rect.Height = Height;
    Rect.Buffer = (UINT64) Buffer;

    return (INT64) LineOSSyscallResult(LineOSSyscall1(SYSCALL_DISPLAY_PRESENT_RECT, (UINT64) &Rect));
}