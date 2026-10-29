// LOSLibC/Include/time.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

typedef long time_t;

struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};

struct tm
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct tm *gmtime_r(const time_t *Timer, struct tm *Result);
