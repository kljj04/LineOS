
// kernel/src/scheduler/LBPWRRChunk.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRRChunk.h>
#include <scheduler/LBPWRRHelper.h>
#include <memory/PMM.h>

STATIC BOOLEAN ChunkInsert(LBPWRR_RUNQUEUE *RunQueue, TASK *Task, UINT8 Weight)
{
    CHUNK *Chunk;
    UINT32 OldCount;
    UINT32 NewCount;
    UINT32 ReadIndex;
    UINT32 OldCurrentIndex;
    UINT32 NewCurrentIndex;
    UINT32 Index;

    if (RunQueue == NULL || Task == NULL || Weight == 0 || Weight > 6)
    {
        return FALSE;
    }

    Chunk = &RunQueue->Chunk;
    OldCount = Chunk->Count;

    if (OldCount > CHUNK_MAX_COUNT || Weight > CHUNK_MAX_COUNT - OldCount)
    {
        return FALSE;
    }

    if (OldCount != 0 && RunQueue->CurrentIndex >= OldCount)
    {
        return FALSE;
    }

    NewCount = OldCount + Weight;
    ReadIndex = OldCount;
    OldCurrentIndex = RunQueue->CurrentIndex;
    NewCurrentIndex = 0;

    for (Index = NewCount; Index > 0; Index--)
    {
        UINT32 Position = Index - 1;
        UINT32 Target = (UINT32) ((UINT64) (Position + 1) * Weight / NewCount);
        UINT32 Previous = (UINT32) ((UINT64) Position * Weight / NewCount);

        if (Target != Previous)
        {
            Chunk->Tasks[Position] = Task;
        }
        else
        {
            ReadIndex--;

            Chunk->Tasks[Position] = Chunk->Tasks[ReadIndex];

            if (ReadIndex == OldCurrentIndex)
            {
                NewCurrentIndex = Position;
            }
        }
    }

    Chunk->Count = NewCount;
    RunQueue->CurrentIndex = NewCurrentIndex;

    return TRUE;
}

STATIC BOOLEAN ChunkRemove(LBPWRR_RUNQUEUE *RunQueue, TASK *Task)
{
    CHUNK  *Chunk;
    UINT32  OldCount;
    UINT32  ReadIndex;
    UINT32  WriteIndex;
    UINT32  CurrentIndex;
    UINT32  NewCurrentIndex;
    BOOLEAN FoundBefore;

    if (RunQueue == NULL || Task == NULL)
    {
        return FALSE;
    }

    Chunk = &RunQueue->Chunk;
    OldCount = Chunk->Count;

    if (OldCount > CHUNK_MAX_COUNT)
    {
        return FALSE;
    }

    if (OldCount != 0 && RunQueue->CurrentIndex >= OldCount)
    {
        return FALSE;
    }

    WriteIndex = 0;
    CurrentIndex = RunQueue->CurrentIndex;
    NewCurrentIndex = 0;
    FoundBefore = FALSE;

    for (ReadIndex = 0; ReadIndex < OldCount; ReadIndex++)
    {
        if (Chunk->Tasks[ReadIndex] == Task)
        {
            continue;
        }

        if (ReadIndex <= CurrentIndex)
        {
            NewCurrentIndex = WriteIndex;
            FoundBefore = TRUE;
        }

        Chunk->Tasks[WriteIndex++] = Chunk->Tasks[ReadIndex];
    }

    for (ReadIndex = WriteIndex; ReadIndex < OldCount; ReadIndex++)
    {
        Chunk->Tasks[ReadIndex] = NULL;
    }

    Chunk->Count = WriteIndex;

    if (WriteIndex == 0)
    {
        RunQueue->CurrentIndex = 0;
    }
    else if (FoundBefore)
    {
        RunQueue->CurrentIndex = NewCurrentIndex;
    }
    else
    {
        RunQueue->CurrentIndex = WriteIndex - 1;
    }

    return TRUE;
}

STATIC BOOLEAN ChunkUpdate(LBPWRR_RUNQUEUE *RunQueue, TASK *Task, UINT8 Weight)
{
    CHUNK  *Chunk;
    UINT32  OldCount;
    UINT32  OldWeight;
    UINT32  ReadIndex;
    UINT32  WriteIndex;
    UINT32  CurrentIndex;
    UINT32  NewCurrentIndex;
    UINT32  SeenCount;
    UINT32  Target;
    UINT32  Previous;
    BOOLEAN FoundBefore;

    if (RunQueue == NULL || Task == NULL || Weight == 0 || Weight > 6)
    {
        return FALSE;
    }

    Chunk = &RunQueue->Chunk;
    OldCount = Chunk->Count;

    if (OldCount > CHUNK_MAX_COUNT)
    {
        return FALSE;
    }

    if (OldCount != 0 && RunQueue->CurrentIndex >= OldCount)
    {
        return FALSE;
    }

    OldWeight = 0;

    for (ReadIndex = 0; ReadIndex < OldCount; ReadIndex++)
    {
        if (Chunk->Tasks[ReadIndex] == Task)
        {
            OldWeight++;
        }
    }

    if (OldWeight == Weight)
    {
        return TRUE;
    }

    if (OldWeight < Weight)
    {
        return ChunkInsert(RunQueue, Task, Weight - OldWeight);
    }

    WriteIndex = 0;
    SeenCount = 0;
    CurrentIndex = RunQueue->CurrentIndex;
    NewCurrentIndex = 0;
    FoundBefore = FALSE;

    for (ReadIndex = 0; ReadIndex < OldCount; ReadIndex++)
    {
        if (Chunk->Tasks[ReadIndex] == Task)
        {
            Target = (SeenCount + 1) * Weight / OldWeight;
            Previous = SeenCount * Weight / OldWeight;
            SeenCount++;

            if (Target == Previous)
            {
                continue;
            }
        }

        if (ReadIndex <= CurrentIndex)
        {
            NewCurrentIndex = WriteIndex;
            FoundBefore = TRUE;
        }

        Chunk->Tasks[WriteIndex++] = Chunk->Tasks[ReadIndex];
    }

    for (ReadIndex = WriteIndex; ReadIndex < OldCount; ReadIndex++)
    {
        Chunk->Tasks[ReadIndex] = NULL;
    }

    Chunk->Count = WriteIndex;

    if (WriteIndex == 0)
    {
        RunQueue->CurrentIndex = 0;
    }
    else if (FoundBefore)
    {
        RunQueue->CurrentIndex = NewCurrentIndex;
    }
    else
    {
        RunQueue->CurrentIndex = WriteIndex - 1;
    }

    return TRUE;
}

BOOLEAN LBPWRRModifyChunk(LBPWRR_RUNQUEUE *RunQueue, TASK *Task, UINT8 Priority)
{
    UINT32  Index;
    BOOLEAN Exists;

    if (RunQueue == NULL || Task == NULL || Priority > 6)
    {
        return FALSE;
    }

    if (RunQueue->Chunk.Count > CHUNK_MAX_COUNT)
    {
        return FALSE;
    }

    if (Priority == 0)
    {
        return ChunkRemove(RunQueue, Task);
    }

    Exists = FALSE;

    for (Index = 0; Index < RunQueue->Chunk.Count; Index++)
    {
        if (RunQueue->Chunk.Tasks[Index] == Task)
        {
            Exists = TRUE;
            break;
        }
    }

    if (!Exists)
    {
        return ChunkInsert(RunQueue, Task, Priority);
    }

    return ChunkUpdate(RunQueue, Task, Priority);
}