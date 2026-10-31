// LOSLibC/Source/Process/process.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

pid_t fork(void)
{
    return (pid_t) LineOSSyscallResult(LineOSSyscall0(SYSCALL_FORK));
}

int execve(const char *Path, char *const Argv[], char *const Envp[])
{
    return (int) LineOSSyscallResult(LineOSSyscall3(SYSCALL_EXECVE, (UINT64) Path, (UINT64) Argv, (UINT64) Envp));
}

pid_t getpid(void)
{
    return (pid_t) LineOSSyscallResult(LineOSSyscall0(SYSCALL_GETPID));
}
