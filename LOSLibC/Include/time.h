// LOSLibC/Include/time.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

typedef long time_t;

struct timespec
{
    time_t tv_sec;
    long   tv_nsec;
};

struct tm
{
    int         tm_sec;
    int         tm_min;
    int         tm_hour;
    int         tm_mday;
    int         tm_mon;
    int         tm_year;
    int         tm_wday;
    int         tm_yday;
    int         tm_isdst;
    long        tm_gmtoff;
    const char *tm_zone;
};

struct tm *gmtime_r(const time_t *Timer, struct tm *Result);
struct tm *localtime_r(const time_t *Timer, struct tm *Result);

#include <Syscall/Time/Time.h>

typedef int clockid_t;

#define CLOCK_REALTIME  LOS_CLOCK_REALTIME
#define CLOCK_MONOTONIC LOS_CLOCK_MONOTONIC

int clock_gettime(clockid_t ClockID, struct timespec *Time);

int nanosleep(const struct timespec *Duration, struct timespec *Remaining);

#include <stddef.h>

size_t strftime(char *Buffer, size_t Capacity, const char *Format, const struct tm *Time);
