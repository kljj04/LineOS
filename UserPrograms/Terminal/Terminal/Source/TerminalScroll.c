// UserPrograms/Terminal/Terminal/Source/TerminalScroll.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TerminalScreen.h>
#include <sys/mman.h>
#include <string.h>

#define TERMINAL_HISTORY_CHUNK_SIZE (1024U * 1024U)

typedef struct TERMINAL_HISTORY_CHUNK
{
    struct TERMINAL_HISTORY_CHUNK *Next;
    UINTN                          Used;
    UINT8                          Data[];
} TERMINAL_HISTORY_CHUNK;

typedef struct TERMINAL_HISTORY_LINE
{
    struct TERMINAL_HISTORY_LINE *Previous;
    struct TERMINAL_HISTORY_LINE *Next;
    TERMINAL_TEXT_LINE            Text;
    TERMINAL_TEXT_CELL            Cells[];
} TERMINAL_HISTORY_LINE;

STATIC TERMINAL_HISTORY_CHUNK *Chunks;
STATIC TERMINAL_HISTORY_CHUNK *CurrentChunk;
STATIC TERMINAL_HISTORY_LINE  *Last;
STATIC TERMINAL_HISTORY_LINE  *Selected;
STATIC UINT64                  HistoryCount;
STATIC UINT64                  Offset;
STATIC UINT32                  LiveRow;
STATIC BOOLEAN                 ScrollDirty;

BOOLEAN TerminalHistorySave(CONST TERMINAL_TEXT_LINE *Line)
{
    UINTN Size = (sizeof(TERMINAL_HISTORY_LINE) + (UINTN) Line->Length * sizeof(TERMINAL_TEXT_CELL) + 15U) & ~(UINTN) 15U;

    if (CurrentChunk == NULL || Size > TERMINAL_HISTORY_CHUNK_SIZE - sizeof(*CurrentChunk) - CurrentChunk->Used)
    {
        TERMINAL_HISTORY_CHUNK *Chunk = mmap(NULL, TERMINAL_HISTORY_CHUNK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (Chunk == MAP_FAILED)
        {
            return FALSE;
        }

        Chunk->Next = NULL;
        Chunk->Used = 0;

        if (CurrentChunk != NULL)
        {
            CurrentChunk->Next = Chunk;
        }
        else
        {
            Chunks = Chunk;
        }

        CurrentChunk = Chunk;
    }

    TERMINAL_HISTORY_LINE *Saved = (TERMINAL_HISTORY_LINE *) (CurrentChunk->Data + CurrentChunk->Used);

    CurrentChunk->Used += Size;
    Saved->Previous = Last;
    Saved->Next = NULL;
    Saved->Text = *Line;
    Saved->Text.Cells = Saved->Cells;
    memcpy(Saved->Cells, Line->Cells, (UINTN) Line->Length * sizeof(TERMINAL_TEXT_CELL));

    if (Last != NULL)
    {
        Last->Next = Saved;
    }

    Last = Saved;
    HistoryCount++;

    if (Offset != 0)
    {
        Offset++;
    }

    return TRUE;
}

VOID TerminalHistorySelect(VOID)
{
    Selected = NULL;
    LiveRow = 0;

    if (Offset != 0)
    {
        Selected = Last;

        for (UINT64 Index = 1; Index < Offset; Index++)
        {
            Selected = Selected->Previous;
        }
    }
}

TERMINAL_TEXT_LINE *TerminalHistoryNext(UINT32 Row)
{
    (VOID) Row;

    if (Selected != NULL)
    {
        TERMINAL_TEXT_LINE *Line = &Selected->Text;

        Selected = Selected->Next;

        return Line;
    }

    return TerminalScreenLine(LiveRow++);
}

VOID TerminalScrollWheel(INT32 Wheel)
{
    UINT64 Previous = Offset;

    if (Wheel < 0)
    {
        UINT64 Amount = (UINT64) - (INT64) Wheel;

        Offset = Amount > Offset ? 0 : Offset - Amount;
    }
    else
    {
        Offset += (UINT32) Wheel;

        if (Offset > HistoryCount)
        {
            Offset = HistoryCount;
        }
    }

    ScrollDirty = ScrollDirty || Previous != Offset;
}

BOOLEAN TerminalScrollActive(VOID)
{
    return Offset != 0;
}

BOOLEAN TerminalScrollTick(UINT64 Now)
{
    (VOID) Now;

    BOOLEAN Changed = ScrollDirty;

    ScrollDirty = FALSE;

    return Changed;
}

BOOLEAN TerminalScrollPointer(DISPLAY_INFO *Info, UINT32 X, UINT32 Y, BOOLEAN Pressed)
{
    STATIC BOOLEAN Dragging;

    if (!Pressed)
    {
        Dragging = FALSE;

        return FALSE;
    }

    if (X >= Info->Width - TERMINAL_SCROLLBAR_WIDTH)
    {
        Dragging = TRUE;
    }

    if (!Dragging)
    {
        return FALSE;
    }

    if (Y >= Info->Height)
    {
        Y = Info->Height - 1;
    }

    UINT64 Previous = Offset;

    Offset = HistoryCount * (Info->Height - 1 - Y) / (Info->Height - 1);
    ScrollDirty = ScrollDirty || Previous != Offset;

    return TRUE;
}

VOID TerminalScrollReset(VOID)
{
    while (Chunks != NULL)
    {
        TERMINAL_HISTORY_CHUNK *Next = Chunks->Next;

        munmap(Chunks, TERMINAL_HISTORY_CHUNK_SIZE);
        Chunks = Next;
    }

    CurrentChunk = NULL;
    Last = NULL;
    Selected = NULL;
    HistoryCount = 0;
    Offset = 0;
    ScrollDirty = TRUE;
}

UINT32 *TerminalScrollView(DISPLAY_INFO *Info)
{
    UINT32 Rows = TerminalScreenRows();
    UINT32 ThumbHeight = (UINT32) ((UINT64) Info->Height * Rows / (HistoryCount + Rows));

    if (ThumbHeight < 16)
    {
        ThumbHeight = 16;
    }

    UINT32 ThumbTop = HistoryCount == 0 ? 0 : (UINT32) ((HistoryCount - Offset) * (Info->Height - ThumbHeight) / HistoryCount);

    STATIC UINT32 PreviousThumbTop = ~0U;
    STATIC UINT32 PreviousThumbHeight;
    STATIC UINT32 PreviousHeight;

    if (ThumbTop != PreviousThumbTop || ThumbHeight != PreviousThumbHeight || Info->Height != PreviousHeight)
    {
        TerminalDamage(0, Info->Height);
        PreviousThumbTop = ThumbTop;
        PreviousThumbHeight = ThumbHeight;
        PreviousHeight = Info->Height;
    }

    for (UINT32 Row = 0; Row < Info->Height; Row++)
    {
        UINT32 Color = Row >= ThumbTop && Row < ThumbTop + ThumbHeight ? 0x00787878U : 0x00292929U;

        for (UINT32 Column = Info->Width - TERMINAL_SCROLLBAR_WIDTH; Column < Info->Width; Column++)
        {
            FrameBuffer[(UINTN) Row * Info->Width + Column] = Color;
        }
    }

    return FrameBuffer;
}
