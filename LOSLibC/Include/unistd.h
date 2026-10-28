// LOSLibC/Include/unistd.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>
#include <sys/types.h>

ssize_t read(int FD, void *Buffer, size_t Count);
int     close(int FD);
ssize_t write(int FD, const void *Buffer, size_t Count);
int     setuid(uid_t UID);
int     setgid(gid_t GID);

pid_t fork(void);
int   execve(const char *Path, char *const Argv[], char *const Envp[]);
pid_t getpid(void);

ssize_t getdents64(int FD, void *Buffer, size_t Count);

int chdir(const char *Path);

char *getcwd(char *Buffer, size_t Size);
