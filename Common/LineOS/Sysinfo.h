// Common/LineOS/Sysinfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

#define SI_LOAD_SHIFT 16

typedef struct
{
    INT64  Uptime;
    UINT64 Loads[3];
    UINT64 TotalRAM;
    UINT64 FreeRAM;
    UINT64 SharedRAM;
    UINT64 BufferRAM;
    UINT64 TotalSwap;
    UINT64 FreeSwap;
    UINT16 Processes;
    UINT16 Padding;
    UINT64 TotalHigh;
    UINT64 FreeHigh;
    UINT32 MemUnit;
    CHAR8  Reserved[0];
} SYS_INFO;

STATIC_ASSERT(sizeof(SYS_INFO) == 112, "Invalid SYS_INFO size");