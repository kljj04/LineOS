// LOSLibC/Source/Process/pthread_sync.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <pthread.h>
#include <errno.h>
#include <syscall.h>

#define FUTEX_WAIT_PRIVATE 128U
#define FUTEX_WAKE_PRIVATE 129U
#define FUTEX_WAKE_ALL     2147483647U
#define MUTEX_LOCKED       1U
#define MUTEX_CONTENDED    2U
#define ONCE_RUNNING       1U
#define ONCE_DONE          2U

STATIC VOID FutexWait(unsigned int *Address, unsigned int Value)
{
    LineOSSyscall6(SYSCALL_FUTEX, (UINT64) Address, FUTEX_WAIT_PRIVATE, Value, 0, 0, 0);
}

STATIC VOID FutexWake(unsigned int *Address, unsigned int Count)
{
    LineOSSyscall6(SYSCALL_FUTEX, (UINT64) Address, FUTEX_WAKE_PRIVATE, Count, 0, 0, 0);
}

int pthread_mutexattr_init(pthread_mutexattr_t *Attr)
{
    Attr->Type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *Attr)
{
    (VOID) Attr;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *Attr, int *Type)
{
    *Type = Attr->Type;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *Attr, int Type)
{
    if (Type != PTHREAD_MUTEX_NORMAL && Type != PTHREAD_MUTEX_RECURSIVE && Type != PTHREAD_MUTEX_ERRORCHECK)
    {
        return EINVAL;
    }

    Attr->Type = Type;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *Mutex, const pthread_mutexattr_t *Attr)
{
    *Mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    Mutex->Type = Attr == NULL ? PTHREAD_MUTEX_DEFAULT : Attr->Type;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *Mutex)
{
    return ATOMIC_LOAD(&Mutex->State, ATOMIC_RELAXED) == 0 ? 0 : EBUSY;
}

STATIC int MutexAcquire(pthread_mutex_t *Mutex, BOOLEAN Try)
{
    UINT32 TID = Mutex->Type == PTHREAD_MUTEX_NORMAL ? 0 : (UINT32) LineOSSyscall0(SYSCALL_GETTID);

    if (TID != 0 && ATOMIC_LOAD(&Mutex->Owner, ATOMIC_RELAXED) == TID)
    {
        if (Mutex->Type == PTHREAD_MUTEX_RECURSIVE)
        {
            if (Mutex->Depth == UINT32_MAX)
            {
                return EAGAIN;
            }

            Mutex->Depth++;
            return 0;
        }

        return Try ? EBUSY : EDEADLK;
    }

    unsigned int Expected = 0;

    if (!ATOMIC_COMPARE_EXCHANGE(&Mutex->State, &Expected, MUTEX_LOCKED, FALSE, ATOMIC_ACQUIRE, ATOMIC_RELAXED))
    {
        if (Try)
        {
            return EBUSY;
        }

        while (ATOMIC_EXCHANGE(&Mutex->State, MUTEX_CONTENDED, ATOMIC_ACQUIRE) != 0)
        {
            FutexWait(&Mutex->State, MUTEX_CONTENDED);
        }
    }

    if (TID != 0)
    {
        Mutex->Depth = 1;
        ATOMIC_STORE(&Mutex->Owner, TID, ATOMIC_RELAXED);
    }

    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *Mutex)
{
    return MutexAcquire(Mutex, TRUE);
}

int pthread_mutex_lock(pthread_mutex_t *Mutex)
{
    return MutexAcquire(Mutex, FALSE);
}

int pthread_mutex_unlock(pthread_mutex_t *Mutex)
{
    if (Mutex->Type != PTHREAD_MUTEX_NORMAL)
    {
        if (ATOMIC_LOAD(&Mutex->Owner, ATOMIC_RELAXED) != (UINT32) LineOSSyscall0(SYSCALL_GETTID))
        {
            return EPERM;
        }

        if (--Mutex->Depth != 0)
        {
            return 0;
        }

        ATOMIC_STORE(&Mutex->Owner, 0, ATOMIC_RELAXED);
    }

    if (ATOMIC_EXCHANGE(&Mutex->State, 0, ATOMIC_RELEASE) == MUTEX_CONTENDED)
    {
        FutexWake(&Mutex->State, 1);
    }

    return 0;
}

int pthread_condattr_init(pthread_condattr_t *Attr)
{
    Attr->Clock = CLOCK_REALTIME;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *Attr)
{
    (VOID) Attr;
    return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t *Attr, clockid_t *Clock)
{
    *Clock = Attr->Clock;
    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *Attr, clockid_t Clock)
{
    if (Clock != CLOCK_REALTIME && Clock != CLOCK_MONOTONIC)
    {
        return EINVAL;
    }

    Attr->Clock = Clock;
    return 0;
}

int pthread_cond_init(pthread_cond_t *Cond, const pthread_condattr_t *Attr)
{
    *Cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    Cond->Clock = Attr == NULL ? CLOCK_REALTIME : Attr->Clock;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *Cond)
{
    return ATOMIC_LOAD(&Cond->Waiters, ATOMIC_RELAXED) == 0 ? 0 : EBUSY;
}

STATIC int CondWait(pthread_cond_t *Cond, pthread_mutex_t *Mutex, const struct timespec *Deadline)
{
    struct timespec Timeout;
    struct timespec Now;
    unsigned int    Sequence = ATOMIC_LOAD(&Cond->Sequence, ATOMIC_ACQUIRE);

    if (Deadline != NULL && (Deadline->tv_nsec < 0 || Deadline->tv_nsec >= 1000000000))
    {
        return EINVAL;
    }

    ATOMIC_FETCH_ADD(&Cond->Waiters, 1, ATOMIC_RELAXED);
    int Result = pthread_mutex_unlock(Mutex);

    if (Result != 0)
    {
        ATOMIC_FETCH_SUB(&Cond->Waiters, 1, ATOMIC_RELAXED);
        return Result;
    }

    if (Deadline == NULL)
    {
        FutexWait(&Cond->Sequence, Sequence);
    }
    else
    {
        INT64 Status = (INT64) LineOSSyscall2(SYSCALL_CLOCK_GETTIME, Cond->Clock, (UINT64) &Now);

        if (Status < 0)
        {
            Result = (int) -Status;
        }
        else if (Deadline->tv_sec < Now.tv_sec || (Deadline->tv_sec == Now.tv_sec && Deadline->tv_nsec <= Now.tv_nsec))
        {
            Result = ETIMEDOUT;
        }
        else
        {
            if (__builtin_sub_overflow(Deadline->tv_sec, Now.tv_sec, &Timeout.tv_sec))
            {
                Timeout.tv_sec = __LONG_MAX__;
            }

            Timeout.tv_nsec = Deadline->tv_nsec - Now.tv_nsec;

            if (Timeout.tv_nsec < 0)
            {
                Timeout.tv_sec--;
                Timeout.tv_nsec += 1000000000;
            }

            Status = (INT64) LineOSSyscall6(SYSCALL_FUTEX, (UINT64) &Cond->Sequence, FUTEX_WAIT_PRIVATE, Sequence, (UINT64) &Timeout, 0, 0);
            Result = Status == -EAGAIN || Status == -EINTR ? 0 : (int) -Status;
        }
    }

    ATOMIC_FETCH_SUB(&Cond->Waiters, 1, ATOMIC_RELAXED);
    int LockResult = pthread_mutex_lock(Mutex);

    return LockResult == 0 ? Result : LockResult;
}

STATIC int CondWaitCancelable(pthread_cond_t *Cond, pthread_mutex_t *Mutex, const struct timespec *Deadline)
{
    if (Deadline != NULL && (Deadline->tv_nsec < 0 || Deadline->tv_nsec >= 1000000000))
    {
        return EINVAL;
    }

    for (;;)
    {
        pthread_testcancel();
        struct timespec Slice;
        INT64           Status = (INT64) LineOSSyscall2(SYSCALL_CLOCK_GETTIME, Cond->Clock, (UINT64) &Slice);

        if (Status < 0)
        {
            return (int) -Status;
        }

        Slice.tv_nsec += 20000000;

        if (Slice.tv_nsec >= 1000000000)
        {
            Slice.tv_sec++;
            Slice.tv_nsec -= 1000000000;
        }

        BOOLEAN      Final = Deadline != NULL && (Deadline->tv_sec < Slice.tv_sec || (Deadline->tv_sec == Slice.tv_sec && Deadline->tv_nsec <= Slice.tv_nsec));
        unsigned int Sequence = ATOMIC_LOAD(&Cond->Sequence, ATOMIC_ACQUIRE);
        int          Result = CondWait(Cond, Mutex, Final ? Deadline : &Slice);
        pthread_testcancel();

        if (Final || Result != ETIMEDOUT)
        {
            return Result;
        }

        if (ATOMIC_LOAD(&Cond->Sequence, ATOMIC_ACQUIRE) != Sequence)
        {
            return 0;
        }
    }
}

int pthread_cond_wait(pthread_cond_t *Cond, pthread_mutex_t *Mutex)
{
    return CondWaitCancelable(Cond, Mutex, NULL);
}

int pthread_cond_timedwait(pthread_cond_t *Cond, pthread_mutex_t *Mutex, const struct timespec *Deadline)
{
    return CondWaitCancelable(Cond, Mutex, Deadline);
}

STATIC int CondWake(pthread_cond_t *Cond, unsigned int Count)
{
    ATOMIC_FETCH_ADD(&Cond->Sequence, 1, ATOMIC_RELEASE);
    FutexWake(&Cond->Sequence, Count);
    return 0;
}

int pthread_cond_signal(pthread_cond_t *Cond)
{
    return CondWake(Cond, 1);
}

int pthread_cond_broadcast(pthread_cond_t *Cond)
{
    return CondWake(Cond, FUTEX_WAKE_ALL);
}

int pthread_barrier_init(pthread_barrier_t *Barrier, const pthread_barrierattr_t *Attr, unsigned int Count)
{
    if (Count == 0)
    {
        return EINVAL;
    }

    if (Attr != NULL)
    {
        return ENOTSUP;
    }

    *Barrier = (pthread_barrier_t) {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, Count, 0, 0};
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *Barrier)
{
    return pthread_cond_destroy(&Barrier->Changed);
}

int pthread_barrier_wait(pthread_barrier_t *Barrier)
{
    pthread_mutex_lock(&Barrier->Lock);

    unsigned int Generation = Barrier->Generation;
    int          Result = 0;

    if (++Barrier->Arrived == Barrier->Count)
    {
        Barrier->Arrived = 0;
        Barrier->Generation++;
        pthread_cond_broadcast(&Barrier->Changed);
        Result = PTHREAD_BARRIER_SERIAL_THREAD;
    }

    while (Generation == Barrier->Generation)
    {
        CondWait(&Barrier->Changed, &Barrier->Lock, NULL);
    }

    pthread_mutex_unlock(&Barrier->Lock);
    return Result;
}

int pthread_rwlock_init(pthread_rwlock_t *Lock, const pthread_rwlockattr_t *Attr)
{
    if (Attr != NULL)
    {
        return ENOTSUP;
    }

    *Lock = (pthread_rwlock_t) PTHREAD_RWLOCK_INITIALIZER;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *Lock)
{
    return Lock->Readers == 0 && Lock->Writers == 0 && Lock->Writing == 0 ? 0 : EBUSY;
}

STATIC int RWLockAcquire(pthread_rwlock_t *Lock, BOOLEAN Write, BOOLEAN Try)
{
    if (Try)
    {
        if (pthread_mutex_trylock(&Lock->Lock) != 0)
        {
            return EBUSY;
        }

        if (Lock->Writing || (Write && Lock->Readers != 0))
        {
            pthread_mutex_unlock(&Lock->Lock);
            return EBUSY;
        }
    }
    else
    {
        pthread_mutex_lock(&Lock->Lock);
    }

    if (Write)
    {
        Lock->Writers++;

        while (Lock->Writing || Lock->Readers != 0)
        {
            CondWait(&Lock->Changed, &Lock->Lock, NULL);
        }

        Lock->Writers--;
        Lock->Writing = 1;
    }
    else
    {
        while (Lock->Writing)
        {
            CondWait(&Lock->Changed, &Lock->Lock, NULL);
        }

        Lock->Readers++;
    }

    pthread_mutex_unlock(&Lock->Lock);
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *Lock)
{
    return RWLockAcquire(Lock, FALSE, FALSE);
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *Lock)
{
    return RWLockAcquire(Lock, FALSE, TRUE);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *Lock)
{
    return RWLockAcquire(Lock, TRUE, FALSE);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *Lock)
{
    return RWLockAcquire(Lock, TRUE, TRUE);
}

int pthread_rwlock_unlock(pthread_rwlock_t *Lock)
{
    pthread_mutex_lock(&Lock->Lock);

    if (Lock->Writing)
    {
        Lock->Writing = 0;
    }
    else if (Lock->Readers != 0)
    {
        Lock->Readers--;
    }
    else
    {
        pthread_mutex_unlock(&Lock->Lock);
        return EPERM;
    }

    if (Lock->Readers == 0)
    {
        pthread_cond_broadcast(&Lock->Changed);
    }

    pthread_mutex_unlock(&Lock->Lock);
    return 0;
}

int pthread_once(pthread_once_t *Once, void (*Init)(void))
{
    if (ATOMIC_LOAD(Once, ATOMIC_ACQUIRE) != ONCE_DONE)
    {
        unsigned int Expected = 0;

        if (ATOMIC_COMPARE_EXCHANGE(Once, &Expected, ONCE_RUNNING, FALSE, ATOMIC_ACQ_REL, ATOMIC_ACQUIRE))
        {
            Init();
            ATOMIC_STORE(Once, ONCE_DONE, ATOMIC_RELEASE);
            FutexWake(Once, FUTEX_WAKE_ALL);
        }
        else
        {
            while (ATOMIC_LOAD(Once, ATOMIC_ACQUIRE) != ONCE_DONE)
            {
                FutexWait(Once, ONCE_RUNNING);
            }
        }
    }

    return 0;
}
