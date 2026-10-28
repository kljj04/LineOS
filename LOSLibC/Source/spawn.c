// LOSLibC/Source/spawn.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <spawn.h>
#include <syscall.h>
#include <errno.h>

pid_t spawn(const char *Path)
{
    return (pid_t) LineOSSyscallResult(LineOSSyscall1(SYSCALL_SPAWN, (UINT64) Path));
}

pid_t spawnv(const char *Path, const char *const Argv[])
{
    return (pid_t) LineOSSyscallResult(LineOSSyscall2(SYSCALL_SPAWNV, (UINT64) Path, (UINT64) Argv));
}
