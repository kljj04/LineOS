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
