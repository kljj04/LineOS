// Libraries/Mesa/Source/src/util/LineOSQueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "util/u_queue.h"
#include <stdlib.h>

bool util_queue_init(struct util_queue *Queue, const char *Name, unsigned MaxJobs, unsigned NumThreads, unsigned Flags, void *GlobalData)
{
    (void) Name;
    (void) NumThreads;
    (void) Flags;

    memset(Queue, 0, sizeof(*Queue));
    if (MaxJobs == 0 || MaxJobs > INT_MAX)
    {
        return false;
    }

    Queue->max_jobs = MaxJobs;
    Queue->global_data = GlobalData;

    return true;
}

void util_queue_destroy(struct util_queue *Queue)
{
    memset(Queue, 0, sizeof(*Queue));
}

void util_queue_add_job(struct util_queue *Queue, void *Job, struct util_queue_fence *Fence, util_queue_execute_func Execute, util_queue_execute_func Cleanup, size_t JobSize)
{
    (void) JobSize;
    void *GlobalData = Queue->global_data;

    if (Fence != NULL)
    {
        util_queue_fence_reset(Fence);
    }

    Execute(Job, GlobalData, 0);

    if (Fence != NULL)
    {
        util_queue_fence_signal(Fence);
    }

    if (Cleanup != NULL)
    {
        Cleanup(Job, GlobalData, 0);
    }
}

void _util_queue_fence_wait(struct util_queue_fence *Fence)
{
    if (!util_queue_fence_is_signalled(Fence))
    {
        abort();
    }
}

bool _util_queue_fence_wait_timeout(struct util_queue_fence *Fence, int64_t Deadline)
{
    (void) Deadline;

    return util_queue_fence_is_signalled(Fence);
}

void util_queue_drop_job(struct util_queue *Queue, struct util_queue_fence *Fence)
{
    (void) Queue;
    util_queue_fence_wait(Fence);
}

void util_queue_finish(struct util_queue *Queue)
{
    (void) Queue;
}

void util_queue_adjust_num_threads(struct util_queue *Queue, unsigned NumThreads, bool Locked)
{
    (void) Queue;
    (void) NumThreads;
    (void) Locked;
}

int64_t util_queue_get_thread_time_nano(struct util_queue *Queue, unsigned ThreadIndex)
{
    (void) Queue;
    (void) ThreadIndex;

    return 0;
}
