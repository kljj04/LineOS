// LOSLibC/Include/pthread.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>
#include <time.h>

typedef struct PTHREAD *pthread_t;

typedef struct PTHREAD_ATTR
{
    size_t StackSize;
    int    DetachState;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE       0
#define PTHREAD_CREATE_DETACHED       1
#define PTHREAD_KEYS_MAX              128U
#define PTHREAD_DESTRUCTOR_ITERATIONS 4U

typedef unsigned int pthread_key_t;

#define PTHREAD_STACK_MIN 16384U

typedef struct PTHREAD_MUTEX
{
    unsigned int State;
    unsigned int Owner;
    unsigned int Depth;
    int          Type;
} pthread_mutex_t;

typedef struct PTHREAD_COND
{
    unsigned int Sequence;
    unsigned int Waiters;
    clockid_t    Clock;
} pthread_cond_t;

typedef struct PTHREAD_BARRIER
{
    pthread_mutex_t Lock;
    pthread_cond_t  Changed;
    unsigned int    Count;
    unsigned int    Arrived;
    unsigned int    Generation;
} pthread_barrier_t;

typedef struct PTHREAD_RWLOCK
{
    pthread_mutex_t Lock;
    pthread_cond_t  Changed;
    unsigned int    Readers;
    unsigned int    Writers;
    unsigned int    Writing;
} pthread_rwlock_t;

typedef unsigned int pthread_once_t;
typedef struct PTHREAD_MUTEXATTR
{
    int Type;
} pthread_mutexattr_t;
typedef struct PTHREAD_CONDATTR
{
    clockid_t Clock;
} pthread_condattr_t;
typedef struct PTHREAD_BARRIERATTR pthread_barrierattr_t;
typedef struct PTHREAD_RWLOCKATTR  pthread_rwlockattr_t;

#define PTHREAD_MUTEX_NORMAL          0
#define PTHREAD_MUTEX_DEFAULT         PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_RECURSIVE       1
#define PTHREAD_MUTEX_ERRORCHECK      2
#define PTHREAD_MUTEX_INITIALIZER     {0, 0, 0, PTHREAD_MUTEX_DEFAULT}
#define PTHREAD_COND_INITIALIZER      {0, 0, CLOCK_REALTIME}
#define PTHREAD_RWLOCK_INITIALIZER    {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, 0}
#define PTHREAD_ONCE_INIT             0
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

#ifdef __cplusplus
extern "C"
{
#endif

    int pthread_attr_init(pthread_attr_t *Attr);
    int pthread_attr_destroy(pthread_attr_t *Attr);
    int pthread_attr_getdetachstate(const pthread_attr_t *Attr, int *DetachState);
    int pthread_attr_setdetachstate(pthread_attr_t *Attr, int DetachState);
    int pthread_attr_getstacksize(const pthread_attr_t *Attr, size_t *StackSize);
    int pthread_attr_setstacksize(pthread_attr_t *Attr, size_t StackSize);

    int       pthread_create(pthread_t *Thread, const pthread_attr_t *Attr, void *(*Start)(void *), void *Arg);
    int       pthread_join(pthread_t Thread, void **Result);
    int       pthread_detach(pthread_t Thread);
    pthread_t pthread_self(void);
    int       pthread_equal(pthread_t Left, pthread_t Right);
    void      pthread_exit(void *Result) __attribute__((__noreturn__));

    int pthread_mutexattr_init(pthread_mutexattr_t *Attr);
    int pthread_mutexattr_destroy(pthread_mutexattr_t *Attr);
    int pthread_mutexattr_gettype(const pthread_mutexattr_t *Attr, int *Type);
    int pthread_mutexattr_settype(pthread_mutexattr_t *Attr, int Type);

    int pthread_condattr_init(pthread_condattr_t *Attr);
    int pthread_condattr_destroy(pthread_condattr_t *Attr);
    int pthread_condattr_getclock(const pthread_condattr_t *Attr, clockid_t *Clock);
    int pthread_condattr_setclock(pthread_condattr_t *Attr, clockid_t Clock);

    int pthread_mutex_init(pthread_mutex_t *Mutex, const pthread_mutexattr_t *Attr);
    int pthread_mutex_destroy(pthread_mutex_t *Mutex);
    int pthread_mutex_lock(pthread_mutex_t *Mutex);
    int pthread_mutex_trylock(pthread_mutex_t *Mutex);
    int pthread_mutex_unlock(pthread_mutex_t *Mutex);

    int pthread_cond_init(pthread_cond_t *Cond, const pthread_condattr_t *Attr);
    int pthread_cond_destroy(pthread_cond_t *Cond);
    int pthread_cond_wait(pthread_cond_t *Cond, pthread_mutex_t *Mutex);
    int pthread_cond_timedwait(pthread_cond_t *Cond, pthread_mutex_t *Mutex, const struct timespec *Deadline);
    int pthread_cond_signal(pthread_cond_t *Cond);
    int pthread_cond_broadcast(pthread_cond_t *Cond);

    int pthread_barrier_init(pthread_barrier_t *Barrier, const pthread_barrierattr_t *Attr, unsigned int Count);
    int pthread_barrier_destroy(pthread_barrier_t *Barrier);
    int pthread_barrier_wait(pthread_barrier_t *Barrier);

    int pthread_rwlock_init(pthread_rwlock_t *Lock, const pthread_rwlockattr_t *Attr);
    int pthread_rwlock_destroy(pthread_rwlock_t *Lock);
    int pthread_rwlock_rdlock(pthread_rwlock_t *Lock);
    int pthread_rwlock_tryrdlock(pthread_rwlock_t *Lock);
    int pthread_rwlock_wrlock(pthread_rwlock_t *Lock);
    int pthread_rwlock_trywrlock(pthread_rwlock_t *Lock);
    int pthread_rwlock_unlock(pthread_rwlock_t *Lock);

    int pthread_once(pthread_once_t *Once, void (*Init)(void));

#define PTHREAD_CANCEL_ENABLE       0
#define PTHREAD_CANCEL_DISABLE      1
#define PTHREAD_CANCEL_DEFERRED     0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED            ((void *) -1)

    int  pthread_cancel(pthread_t Thread);
    int  pthread_setcancelstate(int State, int *OldState);
    int  pthread_setcanceltype(int Type, int *OldType);
    void pthread_testcancel(void);

    int   pthread_key_create(pthread_key_t *Key, void (*Destructor)(void *));
    int   pthread_key_delete(pthread_key_t Key);
    void *pthread_getspecific(pthread_key_t Key);
    int   pthread_setspecific(pthread_key_t Key, const void *Value);

    typedef struct PTHREAD_CLEANUP
    {
        struct PTHREAD_CLEANUP *Previous;
        void                    (*Routine)(void *);
        void                   *Arg;
    } PTHREAD_CLEANUP;

    void PthreadCleanupPush(PTHREAD_CLEANUP *Handler, void (*Routine)(void *), void *Arg);
    void PthreadCleanupPop(PTHREAD_CLEANUP *Handler, int Execute);

#define pthread_cleanup_push(Routine, Arg) \
    { \
        PTHREAD_CLEANUP PthreadHandler; \
        PthreadCleanupPush(&PthreadHandler, (Routine), (Arg));
#define pthread_cleanup_pop(Execute) \
    PthreadCleanupPop(&PthreadHandler, (Execute)); \
    }

#ifdef __cplusplusd
}
#endif
