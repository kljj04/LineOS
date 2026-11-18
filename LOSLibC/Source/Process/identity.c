// LOSLibC/Source/Process/identity.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

int setuid(uid_t UID)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_SETUID, UID));
}

int setgid(gid_t GID)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_SETGID, GID));
}

uid_t getuid(void)
{
    return (uid_t) LineOSSyscallResult(LineOSSyscall0(SYSCALL_GETUID));
}

uid_t geteuid(void)
{
    return (uid_t) LineOSSyscallResult(LineOSSyscall0(SYSCALL_GETEUID));
}
