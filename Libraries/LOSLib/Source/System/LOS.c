// Libraries/LOSLib/Source/System/LOS.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOS.h>
#include "../Internal.h"

INT64 LOSRemove(CONST char *Path)
{
    return LOSSyscallResult(LOSSyscall1(SYSCALL_REMOVE, (UINT64) Path));
}

INT64 LOSParentChdir(CONST char *Path)
{
    return LOSSyscallResult(LOSSyscall1(SYSCALL_PARENT_CHDIR, (UINT64) Path));
}

INT64 LOSSudoCache(UINT64 Operation)
{
    return LOSSyscallResult(LOSSyscall1(SYSCALL_SUDO_CACHE, Operation));
}
