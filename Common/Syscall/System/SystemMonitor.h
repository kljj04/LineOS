// Common/Syscall/System/SystemMonitor.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#define SYSTEM_MONITOR_VERSION 1U
#define SYSTEM_MONITOR_CPUS 256U
#define SYSTEM_MONITOR_TASKS 128U

typedef struct
{
    unsigned int PID;
    unsigned int UID;
    unsigned int CPU;
    unsigned int Usage;
    unsigned int State;
    unsigned int Kernel;
    unsigned int Idle;
    unsigned int Reserved;
    char Name[16];
} SYSTEM_TASK_INFO;

typedef struct
{
    unsigned int Version;
    unsigned int CPUCount;
    unsigned int CPUUsage;
    unsigned int TaskCount;
    unsigned int TasksReturned;
    unsigned int IdleTaskCount;
    unsigned long long UptimeMls;
    unsigned long long ManagedBytes;
    unsigned long long FreeBytes;
    unsigned int CPUs[SYSTEM_MONITOR_CPUS];
    SYSTEM_TASK_INFO Tasks[SYSTEM_MONITOR_TASKS];
    unsigned long long BusyTicks[SYSTEM_MONITOR_CPUS];
    unsigned long long IdleTicks[SYSTEM_MONITOR_CPUS];
} SYSTEM_MONITOR_INFO;
