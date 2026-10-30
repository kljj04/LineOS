// UserPrograms/Shell/Commands/btop/Source/BtopLayout.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Btop.h>

VOID BtopLayout(BTOP *Monitor, UINT32 Columns, UINT32 Rows)
{
    if (Columns > 240)
    {
        Columns = 240;
    }
    if (Rows > 100)
    {
        Rows = 100;
    }
    Monitor->Columns = Columns;
    Monitor->Rows = Rows;
    Monitor->CoreColumns = Columns / 32;
    if (Monitor->CoreColumns == 0)
    {
        Monitor->CoreColumns = 1;
    }
    UINT32 Needed = (Monitor->Info.CPUCount + Monitor->CoreColumns - 1) / Monitor->CoreColumns;
    UINT32 Budget = Rows / 3;
    if (Budget == 0)
    {
        Budget = 1;
    }
    Monitor->CoreRows = Needed < Budget ? Needed : Budget;
    if (Monitor->CoreRows == 0)
    {
        Monitor->CoreRows = 1;
    }
    Monitor->CoresPerPage = Monitor->CoreColumns * Monitor->CoreRows;
    UINT32 Reserved = 16 + Monitor->CoreRows;
    Monitor->TaskRows = Rows > Reserved ? Rows - Reserved : 1;
    if (Monitor->TaskPage * Monitor->TaskRows >= Monitor->Info.TasksReturned)
    {
        Monitor->TaskPage = 0;
    }
    if (Monitor->CorePage * Monitor->CoresPerPage >= Monitor->Info.CPUCount)
    {
        Monitor->CorePage = 0;
    }
}
