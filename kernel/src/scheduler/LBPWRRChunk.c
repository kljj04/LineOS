// kernel/src/scheduler/LBPWRRChunk.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRRChunk.h>
#include <scheduler/LBPWRRHelper.h>
#include <memory/memory.h>

STATIC BOOLEAN LBPWRRBuildChunkInto(LBPWRR_RUNQUEUE *RunQueue, CHUNK *chunk)
{
    INT32  scores[LBPWRR_MAX_TASKS];
    UINT32 TotalWeight;
    UINT32 index;
    UINT32 ChunkIndex;
    UINT32 SelectedIndex;
    INT32  SelectedScore;
    UINT32 weight;
    TASK  *task;

    if (RunQueue == NULL || chunk == NULL)
    {
        return FALSE;
    }

    TotalWeight = 0;

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        task = RunQueue->Runnable[index];

        if (!LBPWRRTaskIsRunnable(task))
        {
            continue;
        }

        weight = task->Weight;

        if (weight == 0)
        {
            continue;
        }

        if (TotalWeight + weight > CHUNK_MAX_COUNT)
        {
            return FALSE;
        }

        TotalWeight += weight;
    }

    if (TotalWeight == 0)
    {
        chunk->Count = 0;

        return TRUE;
    }

    KMemSet(scores, 0, sizeof(scores));

    for (ChunkIndex = 0; ChunkIndex < TotalWeight; ChunkIndex++)
    {
        SelectedIndex = 0;
        SelectedScore = 0;

        for (index = 0; index < RunQueue->RunnableCount; index++)
        {
            task = RunQueue->Runnable[index];

            if (!LBPWRRTaskIsRunnable(task))
            {
                continue;
            }

            weight = task->Weight;

            if (weight == 0)
            {
                continue;
            }

            scores[index] += (INT32) weight;

            if (scores[index] > SelectedScore)
            {
                SelectedScore = scores[index];
                SelectedIndex = index;
            }
        }

        chunk->Tasks[ChunkIndex] = RunQueue->Runnable[SelectedIndex];

        scores[SelectedIndex] -= (INT32) TotalWeight;
    }

    chunk->Count = TotalWeight;

    return TRUE;
}

BOOLEAN LBPWRRBuildChunk(LBPWRR_RUNQUEUE *RunQueue)
{
    if (!LBPWRRBuildChunkInto(RunQueue, &RunQueue->Chunk))
    {
        return FALSE;
    }

    RunQueue->CurrentIndex = 0;

    return TRUE;
}

STATIC BOOLEAN CheckRebuildPending(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32 index;
    UINT32 count;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    count = RunQueue->RunnableCount;

    if (RunQueue->UnrunnableCount > count)
    {
        count = RunQueue->UnrunnableCount;
    }

    for (index = 0; index < count; index++)
    {
        if ((index < RunQueue->RunnableCount && RunQueue->Runnable[index]->Modified) || (index < RunQueue->UnrunnableCount && RunQueue->Unrunnable[index]->Modified))
        {
            RunQueue->RebuildPending = TRUE;
            break;
        }
    }

    return RunQueue->RebuildPending;
}

BOOLEAN LBPWRRRebuildChunk(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32 index;
    UINT32 count;
    UINT64 BuildGeneration;

    if (CheckRebuildPending(RunQueue) == FALSE)
    {
        return FALSE;
    }

    if (RunQueue->RebuildRunning)
    {
        return TRUE;
    }

    RunQueue->RebuildRunning = TRUE;
    BuildGeneration = RunQueue->Generation;

    if (!LBPWRRBuildChunkInto(RunQueue, &RunQueue->BuildChunk))
    {
        RunQueue->FlushReady = FALSE;
        RunQueue->RebuildRunning = FALSE;
        RunQueue->RebuildPending = TRUE;

        return FALSE;
    }

    RunQueue->BuildGeneration = BuildGeneration;
    RunQueue->FlushReady = TRUE;
    RunQueue->RebuildPending = FALSE;
    RunQueue->RebuildRunning = FALSE;

    count = RunQueue->RunnableCount;

    if (RunQueue->UnrunnableCount > count)
    {
        count = RunQueue->UnrunnableCount;
    }

    for (index = 0; index < count; index++)
    {
        if (index < RunQueue->RunnableCount && RunQueue->Runnable[index] != NULL)
        {
            RunQueue->Runnable[index]->Modified = FALSE;
        }

        if (index < RunQueue->UnrunnableCount && RunQueue->Unrunnable[index] != NULL)
        {
            RunQueue->Unrunnable[index]->Modified = FALSE;
        }
    }

    return TRUE;
}

VOID LBPWRRFlushRebuild(LBPWRR_RUNQUEUE *RunQueue, TASK *CurrentTask)
{
    UINT32 index;
    UINT32 OldIndex;

    if (RunQueue == NULL || !RunQueue->FlushReady)
    {
        return;
    }

    if (RunQueue->BuildGeneration != RunQueue->Generation)
    {
        RunQueue->FlushReady = FALSE;
        RunQueue->RebuildPending = TRUE;
        LBPWRRRebuildChunk(RunQueue);

        return;
    }

    OldIndex = RunQueue->CurrentIndex;
    RunQueue->Chunk = RunQueue->BuildChunk;
    RunQueue->FlushReady = FALSE;

    if (RunQueue->Chunk.Count == 0)
    {
        RunQueue->CurrentIndex = 0;
        return;
    }

    if (CurrentTask != NULL)
    {
        for (index = 0; index < RunQueue->Chunk.Count; index++)
        {
            if (RunQueue->Chunk.Tasks[index] == CurrentTask)
            {
                RunQueue->CurrentIndex = index;
                return;
            }
        }
    }

    if (OldIndex >= RunQueue->Chunk.Count)
    {
        RunQueue->CurrentIndex = 0;
    }
    else
    {
        RunQueue->CurrentIndex = OldIndex;
    }
}
