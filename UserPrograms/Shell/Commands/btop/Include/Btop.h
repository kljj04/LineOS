// UserPrograms/Shell/Commands/btop/Include/Btop.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <SystemMonitor.h>

#define BTOP_HISTORY_SIZE 240U

typedef struct
{
    SYSTEM_MONITOR_INFO Info;
    UINT32 History[BTOP_HISTORY_SIZE];
    UINTN HistoryPosition;
    UINT32 Columns;
    UINT32 Rows;
    UINT32 CoreColumns;
    UINT32 CoreRows;
    UINT32 CoresPerPage;
    UINT32 TaskRows;
    UINT32 CorePage;
    UINT32 TaskPage;
    BOOLEAN Paused;
    BOOLEAN SortPID;
    BOOLEAN HaveCounters;
    UINT64 PreviousBusy[SYSTEM_MONITOR_CPUS];
    UINT64 PreviousIdle[SYSTEM_MONITOR_CPUS];
} BTOP;

VOID BtopSort(BTOP *Monitor);
INT32 BtopRender(CONST BTOP *Monitor);

VOID BtopLayout(BTOP *Monitor, UINT32 Columns, UINT32 Rows);
