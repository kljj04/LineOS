// LOSLibC/Source/spawn.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <spawn.h>
#include <syscall.h>

#define SYSCALL_SPAWN  11ULL
#define SYSCALL_SPAWNV 15ULL

pid_t spawn(const char *Path)
{
    return (pid_t) LineOSSyscall1(SYSCALL_SPAWN, (UINT64) Path);
}

pid_t spawnv(const char *Path, const char *const Argv[])
{
    return (pid_t) LineOSSyscall2(SYSCALL_SPAWNV, (UINT64) Path, (UINT64) Argv);
}
