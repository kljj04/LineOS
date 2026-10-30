// LOSLibC/Include/sys/mman.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <stddef.h>
#include <sys/types.h>
#include <Syscall/Memory/Memory.h>

void *mmap(void *Address, size_t Length, int Protection, int Flags, int FD, long Offset);
int   munmap(void *Address, size_t Length);
