// LOSLibC/Include/fcntl.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_ACCMODE 3
#define O_CREAT   0x40
#define O_EXCL    0x80
#define O_TRUNC   0x200
#define O_APPEND  0x400

int open(const char *Path, int Flags, ...);

#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
