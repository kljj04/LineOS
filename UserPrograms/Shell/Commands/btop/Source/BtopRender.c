// UserPrograms/Shell/Commands/btop/Source/BtopRender.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Btop.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>

STATIC char Output[32768];
STATIC char Previous[32768];
STATIC char Changes[65536];
STATIC UINTN PreviousLength;
STATIC char *Target = Output;
STATIC UINTN Capacity = sizeof(Output);
STATIC UINTN OutputLength;
STATIC UINT32 FrameRow;

STATIC VOID BtopCharacter(char Character)
{
    if (OutputLength < Capacity)
    {
        Target[OutputLength++] = Character;
        if (Target == Output && Character == '\n')
        {
            FrameRow++;
        }
    }
}

STATIC VOID BtopText(CONST char *Text)
{
    while (*Text != 0)
    {
        BtopCharacter(*Text++);
    }
}

STATIC VOID BtopNumber(UINT64 Value, UINTN Width)
{
    char Digits[20];
    UINTN Count = 0;
    do
    {
        Digits[Count++] = '0' + Value % 10;
        Value /= 10;
    } while (Value != 0);
    while (Width > Count)
    {
        BtopCharacter(' ');
        Width--;
    }
    while (Count != 0)
    {
        BtopCharacter(Digits[--Count]);
    }
}

STATIC VOID BtopPair(UINT32 Value)
{
    BtopCharacter('0' + Value / 10);
    BtopCharacter('0' + Value % 10);
}

STATIC VOID BtopBar(UINT64 Percent, UINT32 Width)
{
    if (Percent > 100)
    {
        Percent = 100;
    }
    UINT32 Filled = Percent * Width / 100;
    BtopText("\033[38;2;104;203;97m[");
    for (UINT32 Index = 0; Index < Width; Index++)
    {
        BtopCharacter(Index < Filled ? '#' : '-');
    }
    BtopText("]\033[0m ");
    BtopNumber(Percent, 3);
    BtopCharacter('%');
}

VOID BtopSort(BTOP *Monitor)
{
    for (UINT32 Index = 1; Index < Monitor->Info.TasksReturned; Index++)
    {
        SYSTEM_TASK_INFO Entry = Monitor->Info.Tasks[Index];
        UINT32 Position = Index;
        while (Position != 0)
        {
            SYSTEM_TASK_INFO *Previous = &Monitor->Info.Tasks[Position - 1];
            BOOLEAN Move;
            if (Monitor->SortPID)
            {
                Move = Entry.PID < Previous->PID;
            }
            else if (Entry.Idle != Previous->Idle)
            {
                Move = Entry.Idle < Previous->Idle;
            }
            else if (Entry.Usage != Previous->Usage)
            {
                Move = Entry.Usage > Previous->Usage;
            }
            else
            {
                Move = Entry.PID < Previous->PID;
            }
            if (!Move)
            {
                break;
            }
            Monitor->Info.Tasks[Position] = *Previous;
            Position--;
        }
        Monitor->Info.Tasks[Position] = Entry;
    }
}

STATIC VOID BtopTasks(CONST BTOP *Monitor)
{
    STATIC CONST char *States[] = {"ready", "run", "block", "sleep", "exit", "killed"};
    BtopText("\033[38;2;0;134;255m  PID    UID  CPU   CPU% STATE   TYPE NAME\033[0m\n");
    UINT32 Start = Monitor->TaskPage * Monitor->TaskRows;
    UINT32 End = Start + Monitor->TaskRows;
    if (End > Monitor->Info.TasksReturned)
    {
        End = Monitor->Info.TasksReturned;
    }
    for (UINT32 Index = Start; Index < End; Index++)
    {
        CONST SYSTEM_TASK_INFO *Task = &Monitor->Info.Tasks[Index];
        BtopNumber(Task->PID, 5);
        BtopCharacter(' ');
        BtopNumber(Task->UID, 6);
        BtopCharacter(' ');
        BtopNumber(Task->CPU, 4);
        BtopCharacter(' ');
        BtopNumber(Task->Usage, 5);
        BtopText("% ");
        CONST char *State = "?";
        if (Task->State < sizeof(States) / sizeof(States[0]))
        {
            State = States[Task->State];
        }
        UINTN Length = 0;
        while (State[Length] != 0)
        {
            BtopCharacter(State[Length++]);
        }
        while (Length++ < 7)
        {
            BtopCharacter(' ');
        }
        BtopText(Task->Kernel ? "kern " : "user ");
        CONST char *Name = Task->Name;
        if (Name[0] == 0)
        {
            Name = "unnamed";
        }
        UINTN NameLength = 0;
        while (NameLength < 16 && Name[NameLength] != 0)
        {
            char Value = Name[NameLength++];
            BtopCharacter(Value >= 32 && Value < 127 ? Value : '?');
        }
        while (NameLength++ < 16)
        {
            BtopCharacter(' ');
        }
        if (Monitor->Columns >= 90)
        {
            BtopCharacter(' ');
            BtopBar(Task->Usage, Monitor->Columns - 62);
        }
        BtopCharacter('\n');
    }
    for (UINT32 Empty = End - Start; Empty < Monitor->TaskRows; Empty++)
    {
        BtopCharacter('\n');
    }
    BtopText("\nTasks: ");
    UINT32 RegularTasks = Monitor->Info.TaskCount;
    if (RegularTasks >= Monitor->Info.IdleTaskCount)
    {
        RegularTasks -= Monitor->Info.IdleTaskCount;
    }
    BtopNumber(RegularTasks, 0);
    BtopText("  page ");
    BtopNumber(Monitor->TaskPage + 1, 0);
    BtopText("  returned ");
    BtopNumber(Monitor->Info.TasksReturned, 0);
    BtopText(Monitor->SortPID ? "  sort: PID\n" : "  sort: CPU\n");
}

INT32 BtopRender(CONST BTOP *Monitor)
{
    Target = Output;
    Capacity = sizeof(Output);
    OutputLength = 0;
    FrameRow = 0;
    BtopText("\033[38;2;80;227;194mLineOS btop\033[0m  ");
    struct timespec Time;
    struct tm Date;
    if (clock_gettime(CLOCK_REALTIME, &Time) == 0 && localtime_r(&Time.tv_sec, &Date) != NULL)
    {
        BtopPair(Date.tm_hour);
        BtopCharacter(':');
        BtopPair(Date.tm_min);
        BtopCharacter(':');
        BtopPair(Date.tm_sec);
        BtopCharacter(' ');
        BtopText(Date.tm_zone);
    }
    if (Monitor->Paused)
    {
        BtopText("  [PAUSED]");
    }
    BtopText("\n\nCPU  ");
    BtopBar(Monitor->Info.CPUUsage, Monitor->Columns - 28);
    BtopText("  ");
    BtopNumber(Monitor->Info.CPUCount, 0);
    BtopText(" cores\n     ");
    CONST char *Levels = " .:-=+*#%@";
    UINT32 HistoryWidth = Monitor->Columns - 6;
    for (UINTN Index = 0; Index < HistoryWidth; Index++)
    {
        UINTN Position = (Monitor->HistoryPosition + BTOP_HISTORY_SIZE - HistoryWidth + Index) % BTOP_HISTORY_SIZE;
        UINT32 Value = Monitor->History[Position];
        if (Value > 100)
        {
            Value = 100;
        }
        BtopCharacter(Levels[Value * 9 / 100]);
    }
    BtopCharacter('\n');
    UINT32 CoreStart = Monitor->CorePage * Monitor->CoresPerPage;
    UINT32 CoreEnd = CoreStart + Monitor->CoresPerPage;
    if (CoreEnd > Monitor->Info.CPUCount)
    {
        CoreEnd = Monitor->Info.CPUCount;
    }
    for (UINT32 Core = CoreStart; Core < CoreEnd; Core++)
    {
        BtopText("cpu");
        BtopNumber(Core, 3);
        BtopCharacter(' ');
        BtopBar(Monitor->Info.CPUs[Core], Monitor->Columns / Monitor->CoreColumns - 16);
        if ((Core - CoreStart + 1) % Monitor->CoreColumns == 0)
        {
            BtopCharacter('\n');
        }
        else
        {
            BtopText("  ");
        }
    }
    if ((CoreEnd - CoreStart) % Monitor->CoreColumns != 0)
    {
        BtopCharacter('\n');
    }
    UINT32 UsedCoreRows = (CoreEnd - CoreStart + Monitor->CoreColumns - 1) / Monitor->CoreColumns;
    while (UsedCoreRows++ < Monitor->CoreRows)
    {
        BtopCharacter('\n');
    }
    UINT64 Total = Monitor->Info.ManagedBytes;
    UINT64 Free = Monitor->Info.FreeBytes;
    if (Free > Total)
    {
        Free = Total;
    }
    UINT64 Used = Total - Free;
    UINT64 Percent = Total == 0 ? 0 : Used * 100 / Total;
    BtopText("\nPMM  ");
    BtopBar(Percent, Monitor->Columns - 12);
    BtopText("\n     ");
    BtopNumber(Used / (1024 * 1024), 0);
    BtopText(" / ");
    BtopNumber(Total / (1024 * 1024), 0);
    BtopText(" MiB managed  |  up ");
    BtopNumber(Monitor->Info.UptimeMls / 1000, 0);
    BtopText(" s\n");
    BtopText("\033[38;2;215;205;111mIDLE\033[0m ");
    UINT32 Busy = Monitor->Info.CPUUsage;
    if (Busy > 100)
    {
        Busy = 100;
    }
    BtopNumber(100 - Busy, 3);
    BtopText("% idle time  |  ");
    BtopNumber(Monitor->Info.IdleTaskCount, 0);
    BtopText(" idle tasks (excluded)\n\n");
    BtopTasks(Monitor);
    while (FrameRow + 1 < Monitor->Rows)
    {
        BtopCharacter('\n');
    }
    BtopText("q quit | space pause | s sort | n/p tasks | c cores");

    UINTN FrameLength = OutputLength;
    Target = Changes;
    Capacity = sizeof(Changes);
    OutputLength = 0;
    BtopText("\033[?2026h");

    UINTN Current = 0;
    UINTN Old = 0;
    UINT32 Row = 1;
    while (Current < FrameLength || Old < PreviousLength)
    {
        UINTN CurrentEnd = Current;
        UINTN OldEnd = Old;
        while (CurrentEnd < FrameLength && Output[CurrentEnd] != '\n')
        {
            CurrentEnd++;
        }
        while (OldEnd < PreviousLength && Previous[OldEnd] != '\n')
        {
            OldEnd++;
        }
        UINTN CurrentSize = CurrentEnd - Current;
        UINTN OldSize = OldEnd - Old;
        if (CurrentSize != OldSize || memcmp(Output + Current, Previous + Old, CurrentSize) != 0)
        {
            BtopText("\033[");
            BtopNumber(Row, 0);
            BtopText(";1H\033[0m\033[2K");
            for (UINTN Index = Current; Index < CurrentEnd; Index++)
            {
                BtopCharacter(Output[Index]);
            }
        }
        Current = CurrentEnd;
        Old = OldEnd;
        if (Current < FrameLength)
        {
            Current++;
        }
        if (Old < PreviousLength)
        {
            Old++;
        }
        Row++;
    }
    BtopText("\033[?2026l");

    UINTN Position = 0;
    while (Position < OutputLength)
    {
        ssize_t Count = write(1, Changes + Position, OutputLength - Position);
        if (Count < 0 && errno == EINTR)
        {
            continue;
        }
        if (Count <= 0)
        {
            return -1;
        }
        Position += (UINTN) Count;
    }
    memcpy(Previous, Output, FrameLength);
    PreviousLength = FrameLength;
    return 0;
}
