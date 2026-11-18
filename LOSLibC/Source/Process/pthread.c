// LOSLibC/Source/Process/pthread.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <syscall.h>

#define PTHREAD_STACK_DEFAULT (1024U * 1024U)
#define PTHREAD_PAGE_SIZE     4096U
#define PTHREAD_CLONE_FLAGS   0x003D0F00U
#define PTHREAD_JOINING       2

typedef struct PTHREAD_KEY_SLOT
{
    pthread_key_t Key;
    void          (*Destructor)(void *);
    BOOLEAN       Used;
} PTHREAD_KEY_SLOT;

typedef struct PTHREAD_KEY_VALUE
{
    pthread_key_t Key;
    void         *Value;
} PTHREAD_KEY_VALUE;

struct PTHREAD
{
    struct PTHREAD   *Next;
    void             *(*Start)(void *);
    void             *Arg;
    void             *Result;
    size_t            Size;
    UINT32            ID;
    UINT32            TID;
    int               State;
    int               Errno;
    BOOLEAN           CancelPending;
    int               CancelState;
    PTHREAD_CLEANUP  *Cleanup;
    PTHREAD_KEY_VALUE Values[PTHREAD_KEYS_MAX];
};

STATIC pthread_mutex_t  ThreadLock = PTHREAD_MUTEX_INITIALIZER;
STATIC pthread_t        Threads;
STATIC struct PTHREAD   MainThread;
STATIC pthread_mutex_t  KeyLock = PTHREAD_MUTEX_INITIALIZER;
STATIC PTHREAD_KEY_SLOT Keys[PTHREAD_KEYS_MAX];

STATIC_ASSERT(sizeof(struct PTHREAD) <= PTHREAD_PAGE_SIZE, "pthread descriptor fits in one page");

INT64         PthreadClone(UINT64 Flags, void *Stack, UINT32 *ParentTID, UINT32 *ChildTID, int *Errno);
VOID NORETURN PthreadRun(pthread_t Thread);

STATIC VOID ThreadReap(VOID)
{
    pthread_t *Link = &Threads;

    while (*Link != NULL)
    {
        pthread_t Thread = *Link;

        if (Thread->State == PTHREAD_CREATE_DETACHED && ATOMIC_LOAD(&Thread->TID, ATOMIC_ACQUIRE) == 0)
        {
            pthread_t Next = Thread->Next;

            if (Thread->Size == 0 || LineOSSyscall2(SYSCALL_MUNMAP, (UINT64) Thread, Thread->Size) == 0)
            {
                *Link = Next;
                continue;
            }
        }

        Link = &Thread->Next;
    }
}

int pthread_attr_init(pthread_attr_t *Attr)
{
    *Attr = (pthread_attr_t) {PTHREAD_STACK_DEFAULT, PTHREAD_CREATE_JOINABLE};
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *Attr)
{
    (VOID) Attr;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *Attr, int *DetachState)
{
    *DetachState = Attr->DetachState;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *Attr, int DetachState)
{
    if (DetachState != PTHREAD_CREATE_JOINABLE && DetachState != PTHREAD_CREATE_DETACHED)
    {
        return EINVAL;
    }

    Attr->DetachState = DetachState;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *Attr, size_t *StackSize)
{
    *StackSize = Attr->StackSize;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *Attr, size_t StackSize)
{
    if (StackSize < PTHREAD_STACK_MIN || StackSize > (size_t) -1 - 2 * PTHREAD_PAGE_SIZE)
    {
        return EINVAL;
    }

    Attr->StackSize = StackSize;
    return 0;
}

int pthread_create(pthread_t *Thread, const pthread_attr_t *Attr, void *(*Start)(void *), void *Arg)
{
    pthread_attr_t Config;

    pthread_attr_init(&Config);

    if (Attr != NULL && (pthread_attr_setstacksize(&Config, Attr->StackSize) != 0 || pthread_attr_setdetachstate(&Config, Attr->DetachState) != 0))
    {
        return EINVAL;
    }

    size_t Size = ((Config.StackSize + PTHREAD_PAGE_SIZE - 1) & ~(size_t) (PTHREAD_PAGE_SIZE - 1)) + PTHREAD_PAGE_SIZE;
    UINT64 Mapping = LineOSSyscall6(SYSCALL_MMAP, 0, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, (UINT64) -1, 0);

    if (Mapping >= (UINT64) -4095)
    {
        return EAGAIN;
    }

    pthread_t New = (pthread_t) Mapping;
    New->Start = Start;
    New->Arg = Arg;
    New->Size = Size;
    New->State = Config.DetachState;

    pthread_t *Stack = (pthread_t *) ((char *) New + Size) - 1;
    *Stack = New;

    pthread_self();
    pthread_mutex_lock(&ThreadLock);
    ThreadReap();
    INT64 ID = PthreadClone(PTHREAD_CLONE_FLAGS, Stack, &New->TID, &New->TID, &New->Errno);

    if (ID < 0)
    {
        New->State = PTHREAD_CREATE_DETACHED;
        New->TID = 0;
        New->Next = Threads;
        Threads = New;
        ThreadReap();
        pthread_mutex_unlock(&ThreadLock);
        return (int) -ID;
    }

    New->ID = (UINT32) ID;
    New->Next = Threads;
    Threads = New;
    *Thread = New;
    pthread_mutex_unlock(&ThreadLock);
    return 0;
}

pthread_t pthread_self(void)
{
    UINT32 TID = (UINT32) LineOSSyscall0(SYSCALL_GETTID);
    pthread_mutex_lock(&ThreadLock);

    for (pthread_t Thread = Threads; Thread != NULL; Thread = Thread->Next)
    {
        if (Thread->ID == TID && ATOMIC_LOAD(&Thread->TID, ATOMIC_ACQUIRE) != 0)
        {
            pthread_mutex_unlock(&ThreadLock);
            return Thread;
        }
    }

    if (MainThread.ID == 0)
    {
        MainThread.ID = TID;
        MainThread.TID = TID;
        LineOSSyscall1(SYSCALL_SET_TID_ADDRESS, (UINT64) &MainThread.TID);
        MainThread.Next = Threads;
        Threads = &MainThread;
    }

    pthread_mutex_unlock(&ThreadLock);
    return &MainThread;
}

int pthread_equal(pthread_t Left, pthread_t Right)
{
    return Left == Right;
}

int pthread_cancel(pthread_t Thread)
{
    pthread_mutex_lock(&ThreadLock);

    for (pthread_t Current = Threads; Current != NULL; Current = Current->Next)
    {
        if (Current == Thread && ATOMIC_LOAD(&Current->TID, ATOMIC_ACQUIRE) != 0)
        {
            ATOMIC_STORE(&Current->CancelPending, TRUE, ATOMIC_RELEASE);
            pthread_mutex_unlock(&ThreadLock);
            return 0;
        }
    }

    pthread_mutex_unlock(&ThreadLock);
    return ESRCH;
}

int pthread_setcancelstate(int State, int *OldState)
{
    if (State != PTHREAD_CANCEL_ENABLE && State != PTHREAD_CANCEL_DISABLE)
    {
        return EINVAL;
    }

    pthread_t Thread = pthread_self();

    if (OldState != NULL)
    {
        *OldState = Thread->CancelState;
    }

    Thread->CancelState = State;
    return 0;
}

int pthread_setcanceltype(int Type, int *OldType)
{
    if (Type != PTHREAD_CANCEL_DEFERRED)
    {
        return Type == PTHREAD_CANCEL_ASYNCHRONOUS ? ENOTSUP : EINVAL;
    }

    if (OldType != NULL)
    {
        *OldType = PTHREAD_CANCEL_DEFERRED;
    }

    return 0;
}

void pthread_testcancel(void)
{
    pthread_t Thread = pthread_self();

    if (Thread->CancelState == PTHREAD_CANCEL_ENABLE && ATOMIC_LOAD(&Thread->CancelPending, ATOMIC_ACQUIRE))
    {
        Thread->CancelState = PTHREAD_CANCEL_DISABLE;
        pthread_exit(PTHREAD_CANCELED);
    }
}

STATIC void ThreadUndoJoin(void *Arg)
{
    pthread_t Thread = Arg;

    pthread_mutex_lock(&ThreadLock);
    Thread->State = PTHREAD_CREATE_JOINABLE;
    pthread_mutex_unlock(&ThreadLock);
}

int pthread_join(pthread_t Thread, void **Result)
{
    if (Thread == pthread_self())
    {
        return EDEADLK;
    }

    pthread_mutex_lock(&ThreadLock);

    if (Thread->State != PTHREAD_CREATE_JOINABLE)
    {
        pthread_mutex_unlock(&ThreadLock);
        return EINVAL;
    }

    Thread->State = PTHREAD_JOINING;
    pthread_mutex_unlock(&ThreadLock);

    UINT32                TID;
    const struct timespec Slice = {0, 20000000};
    pthread_cleanup_push(ThreadUndoJoin, Thread);
    pthread_testcancel();

    while ((TID = ATOMIC_LOAD(&Thread->TID, ATOMIC_ACQUIRE)) != 0)
    {
        LineOSSyscall6(SYSCALL_FUTEX, (UINT64) &Thread->TID, 128, TID, (UINT64) &Slice, 0, 0);
        pthread_testcancel();
    }

    pthread_cleanup_pop(0);

    if (Result != NULL)
    {
        *Result = __atomic_load_n(&Thread->Result, __ATOMIC_ACQUIRE);
    }

    pthread_mutex_lock(&ThreadLock);
    Thread->State = PTHREAD_CREATE_DETACHED;
    ThreadReap();
    pthread_mutex_unlock(&ThreadLock);
    return 0;
}

int pthread_detach(pthread_t Thread)
{
    pthread_mutex_lock(&ThreadLock);

    if (Thread->State != PTHREAD_CREATE_JOINABLE)
    {
        pthread_mutex_unlock(&ThreadLock);
        return EINVAL;
    }

    Thread->State = PTHREAD_CREATE_DETACHED;
    ThreadReap();
    pthread_mutex_unlock(&ThreadLock);
    return 0;
}

int pthread_key_create(pthread_key_t *Key, void (*Destructor)(void *))
{
    pthread_mutex_lock(&KeyLock);

    for (UINT32 Index = 0; Index < PTHREAD_KEYS_MAX; Index++)
    {
        PTHREAD_KEY_SLOT *Slot = &Keys[Index];

        if (!Slot->Used)
        {
            Slot->Key = (Slot->Key & ~(PTHREAD_KEYS_MAX - 1U)) + PTHREAD_KEYS_MAX + Index;
            Slot->Destructor = Destructor;
            Slot->Used = TRUE;
            *Key = Slot->Key;
            pthread_mutex_unlock(&KeyLock);
            return 0;
        }
    }

    pthread_mutex_unlock(&KeyLock);
    return EAGAIN;
}

int pthread_key_delete(pthread_key_t Key)
{
    PTHREAD_KEY_SLOT *Slot = &Keys[Key % PTHREAD_KEYS_MAX];
    pthread_mutex_lock(&KeyLock);
    BOOLEAN Valid = Slot->Used && Slot->Key == Key;

    if (Valid)
    {
        Slot->Used = FALSE;
    }

    pthread_mutex_unlock(&KeyLock);
    return Valid ? 0 : EINVAL;
}

void *pthread_getspecific(pthread_key_t Key)
{
    UINT32    Index = Key % PTHREAD_KEYS_MAX;
    pthread_t Thread = pthread_self();
    pthread_mutex_lock(&KeyLock);
    void *Value = Keys[Index].Used && Keys[Index].Key == Key && Thread->Values[Index].Key == Key ? Thread->Values[Index].Value : NULL;
    pthread_mutex_unlock(&KeyLock);
    return Value;
}

int pthread_setspecific(pthread_key_t Key, const void *Value)
{
    UINT32    Index = Key % PTHREAD_KEYS_MAX;
    pthread_t Thread = pthread_self();
    pthread_mutex_lock(&KeyLock);
    BOOLEAN Valid = Keys[Index].Used && Keys[Index].Key == Key;

    if (Valid)
    {
        Thread->Values[Index] = (PTHREAD_KEY_VALUE) {Key, (void *) Value};
    }

    pthread_mutex_unlock(&KeyLock);
    return Valid ? 0 : EINVAL;
}

STATIC VOID ThreadDestroyValues(pthread_t Thread)
{
    for (UINT32 Pass = 0; Pass < PTHREAD_DESTRUCTOR_ITERATIONS; Pass++)
    {
        BOOLEAN Called = FALSE;

        for (UINT32 Index = 0; Index < PTHREAD_KEYS_MAX; Index++)
        {
            PTHREAD_KEY_VALUE *Value = &Thread->Values[Index];
            pthread_mutex_lock(&KeyLock);
            void  (*Destructor)(void *) = Keys[Index].Used && Keys[Index].Key == Value->Key ? Keys[Index].Destructor : NULL;
            void *Arg = Value->Value;
            Value->Value = NULL;
            pthread_mutex_unlock(&KeyLock);

            if (Destructor != NULL && Arg != NULL)
            {
                Called = TRUE;
                Destructor(Arg);
            }
        }

        if (!Called)
        {
            break;
        }
    }
}

void PthreadCleanupPush(PTHREAD_CLEANUP *Handler, void (*Routine)(void *), void *Arg)
{
    pthread_t Thread = pthread_self();

    *Handler = (PTHREAD_CLEANUP) {Thread->Cleanup, Routine, Arg};
    Thread->Cleanup = Handler;
}

void PthreadCleanupPop(PTHREAD_CLEANUP *Handler, int Execute)
{
    pthread_self()->Cleanup = Handler->Previous;

    if (Execute)
    {
        Handler->Routine(Handler->Arg);
    }
}

void pthread_exit(void *Result)
{
    pthread_t Thread = pthread_self();

    Thread->CancelState = PTHREAD_CANCEL_DISABLE;

    while (Thread->Cleanup != NULL)
    {
        PthreadCleanupPop(Thread->Cleanup, 1);
    }

    ThreadDestroyValues(Thread);
    __atomic_store_n(&Thread->Result, Result, __ATOMIC_RELEASE);
    LineOSSyscall1(SYSCALL_EXIT, 0);
    __builtin_trap();
}

VOID NORETURN PthreadRun(pthread_t Thread)
{
    pthread_exit(Thread->Start(Thread->Arg));
}
