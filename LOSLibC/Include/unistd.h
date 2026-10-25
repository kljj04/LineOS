// LOSLibC/Include/unistd.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>

typedef long ssize_t;

ssize_t write(int FD, const void *Buffer, size_t Count);