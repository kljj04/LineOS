// LOSLibC/Source/wait.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sys/wait.h>
#include <syscall.h>

pid_t waitpid(pid_t PID, int *Status, int Options)
{
    return (pid_t) LineOSSyscallResult(LineOSSyscall6(SYSCALL_WAIT4, (UINT64) (INT64) PID, (UINT64) Status, (UINT64) Options, 0, 0, 0));
}

pid_t wait(int *Status)
{
    return waitpid(-1, Status, 0);
}
