// LOSLibC/Source/File/ownership.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

int chown(const char *Path, uid_t UID, gid_t GID)
{
    return (int) LineOSSyscallResult(LineOSSyscall3(SYSCALL_CHOWN, (UINT64) Path, UID, GID));
}
