// UserPrograms/Shell/Commands/btop/Source/Btop.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Btop.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>

STATIC BTOP Monitor;

STATIC UINT64 BtopNow(VOID)
{
    struct timespec Time;
    if (clock_gettime(CLOCK_MONOTONIC, &Time) != 0)
    {
        return 0;
    }
    return (UINT64) Time.tv_sec * 1000 + Time.tv_nsec / 1000000;
}

STATIC BOOLEAN BtopSample(VOID)
{
    SYS_INFO System;
    if (sysinfo(&System) != 0 || LOSSystemMonitor(&Monitor.Info) != 0)
    {
        return FALSE;
    }
    Monitor.Info.UptimeMls = (UINT64) System.Uptime * 1000;
    Monitor.Info.ManagedBytes = System.TotalRAM * System.MemUnit;
    Monitor.Info.FreeBytes = System.FreeRAM * System.MemUnit;
    Monitor.Info.TaskCount = System.Processes;
    UINT64 AllBusy = 0;
    UINT64 AllTime = 0;
    for (UINT32 CPU = 0; CPU < Monitor.Info.CPUCount; CPU++)
    {
        UINT64 Busy = Monitor.Info.BusyTicks[CPU];
        UINT64 Idle = Monitor.Info.IdleTicks[CPU];
        Monitor.Info.CPUs[CPU] = 0;
        if (Monitor.HaveCounters && Busy >= Monitor.PreviousBusy[CPU] && Idle >= Monitor.PreviousIdle[CPU])
        {
            UINT64 BusyDelta = Busy - Monitor.PreviousBusy[CPU];
            UINT64 IdleDelta = Idle - Monitor.PreviousIdle[CPU];
            UINT64 Total = BusyDelta + IdleDelta;
            if (Total != 0)
            {
                Monitor.Info.CPUs[CPU] = BusyDelta * 100 / Total;
            }
            AllBusy += BusyDelta;
            AllTime += Total;
        }
        Monitor.PreviousBusy[CPU] = Busy;
        Monitor.PreviousIdle[CPU] = Idle;
    }
    Monitor.Info.CPUUsage = AllTime == 0 ? 0 : AllBusy * 100 / AllTime;
    UINT32 Count = 0;
    for (UINT32 Index = 0; Index < Monitor.Info.TasksReturned; Index++)
    {
        if (!Monitor.Info.Tasks[Index].Idle)
        {
            Monitor.Info.Tasks[Count++] = Monitor.Info.Tasks[Index];
        }
    }
    Monitor.Info.TasksReturned = Count;
    Monitor.HaveCounters = TRUE;
    return TRUE;
}

INT32 main(INT32 Argc, char **Argv)
{
    if (Argc != 1)
    {
        if (Argc == 2 && strcmp(Argv[1], "--help") == 0)
        {
            puts("Usage: btop");
            puts("q quit, space pause, s CPU/PID sort, n/p task pages, c core pages");
            return 0;
        }
        puts("Usage: btop");
        return 1;
    }
    INT32 TTY = open("/dev/lsh1", O_RDONLY);
    if (TTY < 0)
    {
        puts("btop: cannot open terminal");
        return 1;
    }
    struct winsize Size;
    if (ioctl(TTY, TIOCGWINSZ, &Size) < 0 || Size.ws_col < 64 || Size.ws_row < 20)
    {
        close(TTY);
        puts("btop: terminal must be at least 64 columns by 20 rows");
        return 1;
    }
    BtopLayout(&Monitor, Size.ws_col, Size.ws_row);
    CONST char *ClearScreen = "\033[0m\033[2J\033[H\033[?25l";
    write(1, ClearScreen, strlen(ClearScreen));
    UINT64 NextUpdate = 0;
    BOOLEAN Redraw = TRUE;
    INT32 Result = 0;
    for (;;)
    {
        UINT64 Now = BtopNow();
        if (!Monitor.Paused && Now >= NextUpdate)
        {
            if (!BtopSample())
            {
                Result = 1;
                break;
            }
            Monitor.History[Monitor.HistoryPosition] = Monitor.Info.CPUUsage;
            Monitor.HistoryPosition = (Monitor.HistoryPosition + 1) % BTOP_HISTORY_SIZE;
            BtopLayout(&Monitor, Size.ws_col, Size.ws_row);
            BtopSort(&Monitor);
            if (Monitor.TaskPage * Monitor.TaskRows >= Monitor.Info.TasksReturned)
            {
                Monitor.TaskPage = 0;
            }
            NextUpdate = Now + 1000;
            Redraw = TRUE;
        }
        if (Redraw)
        {
            if (BtopRender(&Monitor) < 0)
            {
                Result = 1;
                break;
            }
            Redraw = FALSE;
        }
        INT32 Pending = 0;
        if (ioctl(TTY, FIONREAD, &Pending) < 0)
        {
            Result = 1;
            break;
        }
        if (Pending > 0)
        {
            char Key;
            if (read(TTY, &Key, 1) != 1)
            {
                Result = 1;
                break;
            }
            if (Key == 'q' || Key == 'Q' || Key == 3)
            {
                break;
            }
            switch (Key)
            {
                case ' ':
                {
                    Monitor.Paused = !Monitor.Paused;
                    NextUpdate = 0;
                    break;
                }
                case 's':
                {
                    Monitor.SortPID = !Monitor.SortPID;
                    Monitor.TaskPage = 0;
                    BtopSort(&Monitor);
                    break;
                }
                case 'n':
                {
                    if ((Monitor.TaskPage + 1) * Monitor.TaskRows < Monitor.Info.TasksReturned)
                    {
                        Monitor.TaskPage++;
                    }
                    break;
                }
                case 'p':
                {
                    if (Monitor.TaskPage != 0)
                    {
                        Monitor.TaskPage--;
                    }
                    break;
                }
                case 'c':
                {
                    Monitor.CorePage++;
                    if (Monitor.CorePage * Monitor.CoresPerPage >= Monitor.Info.CPUCount)
                    {
                        Monitor.CorePage = 0;
                    }
                    break;
                }
            }
            Redraw = TRUE;
        }
        struct timespec Delay = {0, 20000000};
        if (nanosleep(&Delay, NULL) < 0)
        {
            Result = 1;
            break;
        }
    }
    close(TTY);
    CONST char *Clear = "\033[?2026l\033[?25h\033[0m\033[2J\033[H";
    write(1, Clear, strlen(Clear));
    if (Result != 0)
    {
        puts("btop: monitor read or output failed");
    }
    return Result;
}
