// LOSLibC/Source/unistd.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>
#include <errno.h>

ssize_t write(int FD, const void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscallResult(LineOSSyscall3(SYSCALL_WRITE, (UINT64) FD, (UINT64) Buffer, (UINT64) Count));
}

ssize_t read(int FD, void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscallResult(LineOSSyscall3(SYSCALL_READ, (UINT64) FD, (UINT64) Buffer, (UINT64) Count));
}

int close(int FD)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_CLOSE, (UINT64) FD));
}

int setuid(uid_t UID)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_SETUID, UID));
}

int setgid(gid_t GID)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_SETGID, GID));
}

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

ssize_t getdents64(int FD, void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscallResult(LineOSSyscall3(SYSCALL_GETDENTS64, (UINT64) FD, (UINT64) Buffer, Count));
}

int chdir(const char *Path)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_CHDIR, (UINT64) Path));
}

char *getcwd(char *Buffer, size_t Size)
{
    return LineOSSyscallResult(LineOSSyscall2(SYSCALL_GETCWD, (UINT64) Buffer, Size)) < 0 ? NULL : Buffer;
}
