// LOSLibC/Include/fcntl.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2

int open(const char *Path, int Flags, ...);
