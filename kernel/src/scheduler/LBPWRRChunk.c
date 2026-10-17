
// kernel/src/scheduler/LBPWRRChunk.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRRChunk.h>
#include <scheduler/LBPWRRHelper.h>
#include <memory/PMM.h>

STATIC BOOLEAN ChunkInsert(LBPWRR_RUNQUEUE *RunQueue, TASK *task, UINT8 weight)
{
    CHUNK *chunk;
    UINT32 OldCount;
    UINT32 NewCount;
    UINT32 ReadIndex;
    UINT32 OldCurrentIndex;
    UINT32 NewCurrentIndex;
    UINT32 index;

    if (RunQueue == NULL || task == NULL || weight == 0 || weight > 6)
    {
        return FALSE;
    }

    chunk = &RunQueue->Chunk;
    OldCount = chunk->Count;

    if (OldCount > CHUNK_MAX_COUNT || weight > CHUNK_MAX_COUNT - OldCount)
    {
        return FALSE;
    }

    if (OldCount != 0 && RunQueue->CurrentIndex >= OldCount)
    {
        return FALSE;
    }

    NewCount = OldCount + weight;
    ReadIndex = OldCount;
    OldCurrentIndex = RunQueue->CurrentIndex;
    NewCurrentIndex = 0;

    for (index = NewCount; index > 0; index--)
    {
        UINT32 position = index - 1;
        UINT32 target = (UINT32) ((UINT64) (position + 1) * weight / NewCount);
        UINT32 previous = (UINT32) ((UINT64) position * weight / NewCount);

        if (target != previous)
        {
            chunk->Tasks[position] = task;
        }
        else
        {
            ReadIndex--;

            chunk->Tasks[position] = chunk->Tasks[ReadIndex];

            if (ReadIndex == OldCurrentIndex)
            {
                NewCurrentIndex = position;
            }
        }
    }

    chunk->Count = NewCount;
    RunQueue->CurrentIndex = NewCurrentIndex;

    return TRUE;
}

STATIC BOOLEAN ChunkRemove(LBPWRR_RUNQUEUE *RunQueue, TASK *task)
{
    CHUNK  *chunk;
    UINT32  OldCount;
    UINT32  ReadIndex;
    UINT32  WriteIndex;
    UINT32  CurrentIndex;
    UINT32  NewCurrentIndex;
    BOOLEAN FoundBefore;

    if (RunQueue == NULL || task == NULL)
    {
        return FALSE;
    }

    chunk = &RunQueue->Chunk;
    OldCount = chunk->Count;

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
        if (chunk->Tasks[ReadIndex] == task)
        {
            continue;
        }

        if (ReadIndex <= CurrentIndex)
        {
            NewCurrentIndex = WriteIndex;
            FoundBefore = TRUE;
        }

        chunk->Tasks[WriteIndex++] = chunk->Tasks[ReadIndex];
    }

    for (ReadIndex = WriteIndex; ReadIndex < OldCount; ReadIndex++)
    {
        chunk->Tasks[ReadIndex] = NULL;
    }

    chunk->Count = WriteIndex;

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

STATIC BOOLEAN ChunkUpdate(LBPWRR_RUNQUEUE *RunQueue, TASK *task, UINT8 weight)
{
    CHUNK  *chunk;
    UINT32  OldCount;
    UINT32  OldWeight;
    UINT32  ReadIndex;
    UINT32  WriteIndex;
    UINT32  CurrentIndex;
    UINT32  NewCurrentIndex;
    UINT32  SeenCount;
    UINT32  target;
    UINT32  previous;
    BOOLEAN FoundBefore;

    if (RunQueue == NULL || task == NULL || weight == 0 || weight > 6)
    {
        return FALSE;
    }

    chunk = &RunQueue->Chunk;
    OldCount = chunk->Count;

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
        if (chunk->Tasks[ReadIndex] == task)
        {
            OldWeight++;
        }
    }

    if (OldWeight == weight)
    {
        return TRUE;
    }

    if (OldWeight < weight)
    {
        return ChunkInsert(RunQueue, task, weight - OldWeight);
    }

    WriteIndex = 0;
    SeenCount = 0;
    CurrentIndex = RunQueue->CurrentIndex;
    NewCurrentIndex = 0;
    FoundBefore = FALSE;

    for (ReadIndex = 0; ReadIndex < OldCount; ReadIndex++)
    {
        if (chunk->Tasks[ReadIndex] == task)
        {
            target = (SeenCount + 1) * weight / OldWeight;
            previous = SeenCount * weight / OldWeight;
            SeenCount++;

            if (target == previous)
            {
                continue;
            }
        }

        if (ReadIndex <= CurrentIndex)
        {
            NewCurrentIndex = WriteIndex;
            FoundBefore = TRUE;
        }

        chunk->Tasks[WriteIndex++] = chunk->Tasks[ReadIndex];
    }

    for (ReadIndex = WriteIndex; ReadIndex < OldCount; ReadIndex++)
    {
        chunk->Tasks[ReadIndex] = NULL;
    }

    chunk->Count = WriteIndex;

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

BOOLEAN LBPWRRModifyChunk(LBPWRR_RUNQUEUE *RunQueue, TASK *task, UINT8 priority)
{
    UINT32  index;
    BOOLEAN exists;

    if (RunQueue == NULL || task == NULL || priority > 6)
    {
        return FALSE;
    }

    if (RunQueue->Chunk.Count > CHUNK_MAX_COUNT)
    {
        return FALSE;
    }

    if (priority == 0)
    {
        return ChunkRemove(RunQueue, task);
    }

    exists = FALSE;

    for (index = 0; index < RunQueue->Chunk.Count; index++)
    {
        if (RunQueue->Chunk.Tasks[index] == task)
        {
            exists = TRUE;
            break;
        }
    }

    if (!exists)
    {
        return ChunkInsert(RunQueue, task, priority);
    }

    return ChunkUpdate(RunQueue, task, priority);
}