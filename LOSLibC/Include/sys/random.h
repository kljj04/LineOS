// LOSLibC/Include/sys/random.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>
#include <sys/types.h>

ssize_t getrandom(void *Buffer, size_t Count, unsigned int Flags);
