
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

    if (RunQueue->RunnableCount > LBPWRR_MAX_TASKS)
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

        if (weight > CHUNK_MAX_COUNT - TotalWeight)
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
        SelectedIndex = LBPWRR_MAX_TASKS;
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

            if (SelectedIndex == LBPWRR_MAX_TASKS || scores[index] > SelectedScore)
            {
                SelectedScore = scores[index];
                SelectedIndex = index;
            }
        }

        if (SelectedIndex == LBPWRR_MAX_TASKS)
        {
            return FALSE;
        }

        chunk->Tasks[ChunkIndex] = RunQueue->Runnable[SelectedIndex];

        scores[SelectedIndex] -= (INT32) TotalWeight;
    }

    chunk->Count = TotalWeight;

    return TRUE;
}

BOOLEAN LBPWRRBuildChunk(LBPWRR_RUNQUEUE *RunQueue)
{
    if (RunQueue == NULL)
    {
        return FALSE;
    }

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
    TASK  *task;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    if (RunQueue->RebuildPending)
    {
        return TRUE;
    }

    if (RunQueue->RunnableCount > LBPWRR_MAX_TASKS || RunQueue->UnrunnableCount > LBPWRR_MAX_TASKS)
    {
        return FALSE;
    }

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        task = RunQueue->Runnable[index];

        if (task != NULL && task->Modified)
        {
            RunQueue->RebuildPending = TRUE;
            return TRUE;
        }
    }

    for (index = 0; index < RunQueue->UnrunnableCount; index++)
    {
        task = RunQueue->Unrunnable[index];

        if (task != NULL && task->Modified)
        {
            RunQueue->RebuildPending = TRUE;
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN LBPWRRRebuildChunk(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32 index;
    UINT64 BuildGeneration;
    TASK  *task;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    if (RunQueue->RunnableCount > LBPWRR_MAX_TASKS || RunQueue->UnrunnableCount > LBPWRR_MAX_TASKS)
    {
        return FALSE;
    }

    if (!CheckRebuildPending(RunQueue))
    {
        return RunQueue->FlushReady;
    }

    if (RunQueue->RebuildRunning)
    {
        return FALSE;
    }

    RunQueue->RebuildRunning = TRUE;
    BuildGeneration = RunQueue->Generation;

    if (!LBPWRRBuildChunkInto(RunQueue, &RunQueue->BuildChunk))
    {
        RunQueue->FlushReady = FALSE;
        RunQueue->RebuildPending = TRUE;
        RunQueue->RebuildRunning = FALSE;

        return FALSE;
    }

    if (BuildGeneration != RunQueue->Generation)
    {
        RunQueue->FlushReady = FALSE;
        RunQueue->RebuildPending = TRUE;
        RunQueue->RebuildRunning = FALSE;

        return FALSE;
    }

    RunQueue->BuildGeneration = BuildGeneration;
    RunQueue->FlushReady = TRUE;
    RunQueue->RebuildPending = FALSE;
    RunQueue->RebuildRunning = FALSE;

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        task = RunQueue->Runnable[index];

        if (task != NULL)
        {
            task->Modified = FALSE;
        }
    }

    for (index = 0; index < RunQueue->UnrunnableCount; index++)
    {
        task = RunQueue->Unrunnable[index];

        if (task != NULL)
        {
            task->Modified = FALSE;
        }
    }

    return TRUE;
}

VOID LBPWRRFlushRebuild(LBPWRR_RUNQUEUE *RunQueue, TASK *CurrentTask)
{
    UINT32 index;
    UINT32 OldIndex;
    UINT32 BestIndex;
    UINT32 BestDistance;
    UINT32 Distance;

    if (RunQueue == NULL || !RunQueue->FlushReady)
    {
        return;
    }

    if (RunQueue->BuildGeneration != RunQueue->Generation)
    {
        RunQueue->FlushReady = FALSE;
        RunQueue->RebuildPending = TRUE;

        if (!LBPWRRRebuildChunk(RunQueue))
        {
            return;
        }

        if (!RunQueue->FlushReady || RunQueue->BuildGeneration != RunQueue->Generation)
        {
            return;
        }
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
        BestIndex = RunQueue->Chunk.Count;
        BestDistance = RunQueue->Chunk.Count;

        for (index = 0; index < RunQueue->Chunk.Count; index++)
        {
            if (RunQueue->Chunk.Tasks[index] != CurrentTask)
            {
                continue;
            }

            if (index >= OldIndex)
            {
                Distance = index - OldIndex;
            }
            else
            {
                Distance = RunQueue->Chunk.Count - OldIndex + index;
            }

            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                BestIndex = index;
            }
        }

        if (BestIndex < RunQueue->Chunk.Count)
        {
            RunQueue->CurrentIndex = BestIndex;
            return;
        }
    }

    RunQueue->CurrentIndex = OldIndex % RunQueue->Chunk.Count;
}
