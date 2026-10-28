// LOSLibC/Source/ioctl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sys/ioctl.h>
#include <syscall.h>
#include <stdarg.h>

int ioctl(int FD, unsigned long Request, ...)
{
    UINT64 Argument = 0;
    if (_IOC_DIR(Request) != 0 || Request == FBIOGET_VSCREENINFO || Request == FBIOGET_FSCREENINFO)
    {
        va_list Arguments;
        va_start(Arguments, Request);
        Argument = (UINT64) va_arg(Arguments, void *);
        va_end(Arguments);
    }
    return (int) LineOSSyscallResult(LineOSSyscall3(SYSCALL_IOCTL, (UINT64) FD, Request, Argument));
}
