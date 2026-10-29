// Common/LineOS/Time.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

#define LOS_UTIME_NOW 1073741823L
#define LOS_UTIME_OMIT 1073741822L

typedef struct
{
    INT64 Seconds;
    INT64 Nanoseconds;
} TIME_SPEC;
