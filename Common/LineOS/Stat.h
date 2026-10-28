// Common/LineOS/Stat.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

typedef struct
{
    unsigned long Device;
    unsigned long Inode;
    unsigned long Links;
    unsigned int  Mode;
    unsigned int  UID;
    unsigned int  GID;
    unsigned int  Padding;
    unsigned long RDevice;
    long          Size;
    long          BlockSize;
    long          Blocks;
    unsigned long AccessTime;
    unsigned long AccessNanoseconds;
    unsigned long ModifyTime;
    unsigned long ModifyNanoseconds;
    unsigned long ChangeTime;
    unsigned long ChangeNanoseconds;
    long          Reserved[3];
} STAT_INFO;
