// LOSLibC/Source/Process/yield.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sched.h>
#include <syscall.h>

int sched_yield(VOID)
{
    return (int) LineOSSyscallResult(LineOSSyscall0(SYSCALL_YIELD));
}
