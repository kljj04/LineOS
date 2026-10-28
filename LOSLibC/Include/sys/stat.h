// LOSLibC/Include/sys/stat.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <sys/types.h>

int mkdir(const char *Path, mode_t Mode);

#include <LineOS/Stat.h>

#define S_IFMT        0170000U
#define S_IFDIR       0040000U
#define S_IFCHR       0020000U
#define S_IFREG       0100000U
#define S_IFLNK       0120000U
#define S_ISDIR(Mode) (((Mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(Mode) (((Mode) & S_IFMT) == S_IFCHR)
#define S_ISREG(Mode) (((Mode) & S_IFMT) == S_IFREG)
#define S_ISLNK(Mode) (((Mode) & S_IFMT) == S_IFLNK)

struct stat
{
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    unsigned int  st_mode;
    unsigned int  st_uid;
    unsigned int  st_gid;
    unsigned int  Padding;
    unsigned long st_rdev;
    long          st_size;
    long          st_blksize;
    long          st_blocks;
    unsigned long st_atime;
    unsigned long AccessNanoseconds;
    unsigned long st_mtime;
    unsigned long ModifyNanoseconds;
    unsigned long st_ctime;
    unsigned long ChangeNanoseconds;
    long          Reserved[3];
};

int stat(const char *Path, struct stat *Info);
int lstat(const char *Path, struct stat *Info);
int fstat(int FD, struct stat *Info);
