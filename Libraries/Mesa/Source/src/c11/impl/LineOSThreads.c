// Libraries/Mesa/Source/src/c11/impl/LineOSThreads.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "c11/threads.h"
#include <stdbool.h>

void call_once(once_flag *Flag, void (*Function)(void))
{
    if (*Flag == 0)
    {
        *Flag = 1;
        Function();
    }
}

int mtx_init(mtx_t *Mutex, int Type)
{
    *Mutex = (mtx_t) {.Type = Type};

    return thrd_success;
}

void mtx_destroy(mtx_t *Mutex)
{
    (void) Mutex;
}

int mtx_trylock(mtx_t *Mutex)
{
    if (Mutex->Depth != 0 && !(Mutex->Type & mtx_recursive))
    {
        return thrd_busy;
    }

    Mutex->Depth++;

    return thrd_success;
}

int mtx_lock(mtx_t *Mutex)
{
    return mtx_trylock(Mutex) == thrd_success ? thrd_success : thrd_error;
}

int mtx_unlock(mtx_t *Mutex)
{
    if (Mutex->Depth == 0)
    {
        return thrd_error;
    }

    Mutex->Depth--;

    return thrd_success;
}

int mtx_timedlock(mtx_t *Mutex, const struct timespec *Deadline)
{
    (void) Deadline;

    return mtx_trylock(Mutex) == thrd_success ? thrd_success : thrd_timedout;
}

int cnd_init(cnd_t *Condition)
{
    *Condition = 0;

    return thrd_success;
}

void cnd_destroy(cnd_t *Condition)
{
    (void) Condition;
}

int cnd_signal(cnd_t *Condition)
{
    (void) Condition;

    return thrd_success;
}

int cnd_broadcast(cnd_t *Condition)
{
    return cnd_signal(Condition);
}

int cnd_wait(cnd_t *Condition, mtx_t *Mutex)
{
    (void) Condition;
    (void) Mutex;

    return thrd_error;
}

int cnd_timedwait(cnd_t *Condition, mtx_t *Mutex, const struct timespec *Deadline)
{
    (void) Deadline;

    return cnd_wait(Condition, Mutex);
}

int thrd_create(thrd_t *Thread, thrd_start_t Start, void *Argument)
{
    (void) Thread;
    (void) Start;
    (void) Argument;

    return thrd_error;
}

thrd_t thrd_current(void)
{
    return 1;
}

int thrd_equal(thrd_t Left, thrd_t Right)
{
    return Left == Right;
}

int thrd_detach(thrd_t Thread)
{
    (void) Thread;

    return thrd_error;
}

int thrd_join(thrd_t Thread, int *Result)
{
    (void) Thread;
    (void) Result;

    return thrd_error;
}

void thrd_exit(int Status)
{
    exit(Status);
}

int thrd_sleep(const struct timespec *Duration, struct timespec *Remaining)
{
    if (nanosleep(Duration, Remaining) == 0)
    {
        return 0;
    }

    return errno == EINTR ? -1 : -2;
}

void thrd_yield(void)
{
    const struct timespec Duration = {0};
    nanosleep(&Duration, NULL);
}

#define MESA_TSS_LIMIT 64

static struct
{
    bool Used;
    void *Value;
    tss_dtor_t Destructor;
} Slots[MESA_TSS_LIMIT];

static void DestroyTSS(void)
{
    for (unsigned int Index = 0; Index < MESA_TSS_LIMIT; Index++)
    {
        if (Slots[Index].Used && Slots[Index].Value != NULL && Slots[Index].Destructor != NULL)
        {
            void *Value = Slots[Index].Value;
            Slots[Index].Value = NULL;
            Slots[Index].Destructor(Value);
        }
    }
}

int tss_create(tss_t *Key, tss_dtor_t Destructor)
{
    static bool Registered;

    if (!Registered)
    {
        if (atexit(DestroyTSS) != 0)
        {
            return thrd_error;
        }

        Registered = true;
    }

    for (unsigned int Index = 0; Index < MESA_TSS_LIMIT; Index++)
    {
        if (!Slots[Index].Used)
        {
            Slots[Index].Used = true;
            Slots[Index].Destructor = Destructor;
            *Key = Index;

            return thrd_success;
        }
    }

    return thrd_error;
}

void tss_delete(tss_t Key)
{
    if (Key < MESA_TSS_LIMIT)
    {
        Slots[Key].Used = false;
        Slots[Key].Value = NULL;
        Slots[Key].Destructor = NULL;
    }
}

void *tss_get(tss_t Key)
{
    return Key < MESA_TSS_LIMIT && Slots[Key].Used ? Slots[Key].Value : NULL;
}

int tss_set(tss_t Key, void *Value)
{
    if (Key >= MESA_TSS_LIMIT || !Slots[Key].Used)
    {
        return thrd_error;
    }

    Slots[Key].Value = Value;

    return thrd_success;
}
