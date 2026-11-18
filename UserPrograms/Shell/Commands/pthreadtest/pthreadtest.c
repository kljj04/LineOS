// UserPrograms/Shell/Commands/pthreadtest/pthreadtest.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#define THREAD_COUNT 4
#define ITERATIONS   1000

static pthread_mutex_t  Mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   Ready = PTHREAD_COND_INITIALIZER;
static pthread_rwlock_t DataLock = PTHREAD_RWLOCK_INITIALIZER;
static pthread_key_t    Key;
static int              Started;
static int              Destructed;
static int              Released;
static int              Counter;

static void DestroyValue(void *Value)
{
    if (Value != NULL)
    {
        pthread_mutex_lock(&Mutex);
        Destructed++;
        pthread_mutex_unlock(&Mutex);
    }
}

static void *Worker(void *Arg)
{
    errno = ERANGE;
    int Result = pthread_setspecific(Key, Arg);

    pthread_mutex_lock(&Mutex);
    Started++;
    pthread_cond_broadcast(&Ready);

    while (!Released)
    {
        pthread_cond_wait(&Ready, &Mutex);
    }

    pthread_mutex_unlock(&Mutex);

    for (int Index = 0; Index < ITERATIONS; Index++)
    {
        pthread_rwlock_wrlock(&DataLock);
        Counter++;
        pthread_rwlock_unlock(&DataLock);
    }

    return Result == 0 && pthread_getspecific(Key) == Arg && errno == ERANGE ? Arg : NULL;
}

static void UnlockCleanup(void *Arg)
{
    pthread_mutex_unlock(Arg);
}

static void *CancelWorker(void *Arg)
{
    pthread_mutex_t *Lock = Arg;

    pthread_mutex_lock(Lock);
    pthread_cleanup_push(UnlockCleanup, Lock);

    for (;;)
    {
        pthread_cond_wait(&Ready, Lock);
    }

    pthread_cleanup_pop(1);
    return NULL;
}

static int VerifyTimeoutAndCancel(void)
{
    pthread_mutexattr_t   Attr;
    pthread_mutex_t       Lock;
    pthread_t             Thread;
    void                 *Result = NULL;
    const struct timespec Deadline = {0, 0};

    if (pthread_mutexattr_init(&Attr) != 0 || pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE) != 0 || pthread_mutex_init(&Lock, &Attr) != 0 || pthread_mutex_lock(&Lock) != 0 || pthread_mutex_trylock(&Lock) != 0 || pthread_mutex_unlock(&Lock) != 0 || pthread_mutex_unlock(&Lock) != 0 || pthread_mutex_unlock(&Lock) != EPERM)
    {
        return 1;
    }

    if (pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_ERRORCHECK) != 0 || pthread_mutex_destroy(&Lock) != 0 || pthread_mutex_init(&Lock, &Attr) != 0 || pthread_mutex_lock(&Lock) != 0 || pthread_mutex_lock(&Lock) != EDEADLK || pthread_cond_timedwait(&Ready, &Lock, &Deadline) != ETIMEDOUT || pthread_mutex_unlock(&Lock) != 0)
    {
        return 1;
    }

    if (pthread_create(&Thread, NULL, CancelWorker, &Lock) != 0 || pthread_cancel(Thread) != 0 || pthread_join(Thread, &Result) != 0 || Result != PTHREAD_CANCELED || pthread_mutex_trylock(&Lock) != 0)
    {
        return 1;
    }

    pthread_mutex_unlock(&Lock);
    pthread_mutex_destroy(&Lock);
    pthread_mutexattr_destroy(&Attr);
    return 0;
}

int main(void)
{
    pthread_t Threads[THREAD_COUNT];
    int       Args[THREAD_COUNT];
    int       Created = 0;
    int       Failed = pthread_key_create(&Key, DestroyValue);

    if (Failed != 0)
    {
        printf("pthreadtest: key creation failed (%d)\n", Failed);
        return 1;
    }

    errno = EINVAL;

    for (; Created < THREAD_COUNT; Created++)
    {
        Args[Created] = Created;
        Failed = pthread_create(&Threads[Created], NULL, Worker, &Args[Created]);

        if (Failed != 0)
        {
            printf("pthreadtest: creation failed (%d)\n", Failed);
            break;
        }
    }

    pthread_mutex_lock(&Mutex);

    while (Started != Created)
    {
        pthread_cond_wait(&Ready, &Mutex);
    }

    Released = 1;
    pthread_cond_broadcast(&Ready);
    pthread_mutex_unlock(&Mutex);

    for (int Index = 0; Index < Created; Index++)
    {
        void *Result = NULL;

        if (pthread_join(Threads[Index], &Result) != 0 || Result != &Args[Index])
        {
            Failed = 1;
        }
    }

    pthread_key_delete(Key);
    pthread_rwlock_rdlock(&DataLock);
    int Total = Counter;
    pthread_rwlock_unlock(&DataLock);

    if (Failed || Total != THREAD_COUNT * ITERATIONS || Destructed != THREAD_COUNT || errno != EINVAL)
    {
        printf("pthreadtest: FAIL (counter=%d, destructors=%d, errno=%d)\n", Total, Destructed, errno);
        return 1;
    }

    if (VerifyTimeoutAndCancel() != 0)
    {
        printf("pthreadtest: FAIL (mutex attributes, timeout, or cancellation)\n");
        return 1;
    }

    printf("pthreadtest: PASS\n");
    return 0;
}
