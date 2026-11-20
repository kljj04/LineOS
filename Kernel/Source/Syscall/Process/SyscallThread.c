// Kernel/Source/Syscall/Process/SyscallThread.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Process/SyscallThread.h>
#include <Syscall/Process/SyscallProcess.h>
#include <Syscall/Process/ProcessInternal.h>
#include <Scheduler/Task.h>
#include <Memory/PMM.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/WaitQueue.h>
#include <Arch/AMD64/CPU.h>
#include <Syscall/Errno.h>
#include <Syscall/Time/Time.h>
#include <Timer/TSC.h>

#define IA32_FS_BASE       0xC0000100U
#define ARCH_SET_FS        0x1002U
#define ARCH_GET_FS        0x1003U
#define FUTEX_WAIT         0U
#define FUTEX_WAKE         1U
#define FUTEX_PRIVATE_FLAG 128U

typedef struct FUTEX_KEY
{
    PROCESS *Process;
    UINT64   Address;
} FUTEX_KEY;

STATIC SPIN_LOCK  FutexLock;
STATIC WAIT_QUEUE FutexWaiters;

STATIC BOOLEAN FutexMatch(TASK *Task, CONST VOID *Context)
{
    CONST FUTEX_KEY *Key = Context;

    return Task->Process == Key->Process && Task->FutexAddress == Key->Address;
}

#define CLONE_VM             0x00000100ULL
#define CLONE_FS             0x00000200ULL
#define CLONE_FILES          0x00000400ULL
#define CLONE_SIGHAND        0x00000800ULL
#define CLONE_THREAD         0x00010000ULL
#define CLONE_SYSVSEM        0x00040000ULL
#define CLONE_SETTLS         0x00080000ULL
#define CLONE_PARENT_SETTID  0x00100000ULL
#define CLONE_CHILD_CLEARTID 0x00200000ULL
#define CLONE_CHILD_SETTID   0x01000000ULL
#define CLONE_DETACHED       0x00400000ULL
#define CLONE_REQUIRED       (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD)
#define CLONE_ALLOWED        (CLONE_REQUIRED | CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID | CLONE_DETACHED)
#define CLONE_ARGS_MIN_SIZE  64U

typedef struct CLONE_ARGS
{
    UINT64 Flags;
    UINT64 PIDFD;
    UINT64 ChildTID;
    UINT64 ParentTID;
    UINT64 ExitSignal;
    UINT64 Stack;
    UINT64 StackSize;
    UINT64 TLS;
    UINT64 SetTID;
    UINT64 SetTIDSize;
    UINT64 Cgroup;
} CLONE_ARGS;

STATIC VOID CloneUnusedEntry(VOID) {}

STATIC INT64 CloneThread(TASK *Parent, INTERRUPT_FRAME *Frame, CONST CLONE_ARGS *Args, UINT64 Stack)
{
    if ((Args->Flags & CLONE_REQUIRED) != CLONE_REQUIRED || (Args->Flags & ~CLONE_ALLOWED) != 0 || Args->ExitSignal != 0)
    {
        return -ENOSYS;
    }

    if (Stack == 0 || Stack >= (1ULL << 47) || Frame->RIP >= (1ULL << 47) || ((Args->Flags & CLONE_SETTLS) != 0 && Args->TLS >= (1ULL << 47)))
    {
        return -EINVAL;
    }

    if (!ProcessBegin())
    {
        return -EAGAIN;
    }

    if (ATOMIC_LOAD(&Parent->Process->GroupExiting, ATOMIC_ACQUIRE))
    {
        return ProcessEnd(-EINTR);
    }

    ProcessReapThreads(Parent->Process);

    TASK *Child = TaskCreateThread(Parent, CloneUnusedEntry);

    if (Child == NULL)
    {
        return ProcessEnd(-EAGAIN);
    }

    ASM("fxsave64 %0" : "=m"(Child->FPState)::"memory");
    Child->FSBase          = (Args->Flags & CLONE_SETTLS) != 0 ? Args->TLS : ReadMSR(IA32_FS_BASE);
    Child->ClearTIDAddress = (Args->Flags & CLONE_CHILD_CLEARTID) != 0 ? Args->ChildTID : 0;
    KMemCpy(&Child->Context, Frame, sizeof(Child->Context));
    Child->Context.RSP    = Stack;
    Child->Context.RAX    = 0;
    Child->Context.RFLAGS = (Frame->RFLAGS & 0x240DD5) | 0x202;
    Child->Context.RCX    = Frame->RIP;
    Child->Context.R11    = Child->Context.RFLAGS;
    KMemCpy((VOID *) Child->RSP, &Child->Context, sizeof(Child->Context));

    UINT32 TID    = Child->TID;
    INT64  Result = -EFAULT;

    if (((Args->Flags & CLONE_PARENT_SETTID) == 0 || VMMWriteUserWord(Parent->Process->AddressSpace, Args->ParentTID, TID)) && ((Args->Flags & CLONE_CHILD_SETTID) == 0 || VMMWriteUserWord(Parent->Process->AddressSpace, Args->ChildTID, TID)))
    {
        if (LBPWRRAddTask(Child))
        {
            return ProcessEnd(TID);
        }

        Result = -EAGAIN;
    }

    TaskKill(Child);
    TaskDestroy(Child);

    return ProcessEnd(Result);
}

INT64 SyscallClone(INTERRUPT_FRAME *Frame)
{
    TASK *Parent = LBPWRRGetCurrentTask();

    if (Parent == NULL || Parent->Process->AddressSpace == NULL || Parent->Process->Execution == NULL)
    {
        return -ESRCH;
    }

    CLONE_ARGS Args = {.Flags = Frame->RDI, .ParentTID = Frame->RDX, .ChildTID = Frame->R10, .TLS = Frame->R8};

    return CloneThread(Parent, Frame, &Args, Frame->RSI);
}

INT64 SyscallClone3(INTERRUPT_FRAME *Frame)
{
    TASK      *Parent = LBPWRRGetCurrentTask();
    UINT64     Size   = Frame->RSI;
    CLONE_ARGS Args   = {0};

    if (Parent == NULL || Parent->Process->AddressSpace == NULL || Parent->Process->Execution == NULL)
    {
        return -ESRCH;
    }

    if (Size < CLONE_ARGS_MIN_SIZE)
    {
        return -EINVAL;
    }

    if (Size > sizeof(Args))
    {
        return -E2BIG;
    }

    if (!VMMCopyFromUser(Parent->Process->AddressSpace, &Args, Frame->RDI, Size))
    {
        return -EFAULT;
    }

    if (Args.PIDFD != 0 || Args.SetTID != 0 || Args.SetTIDSize != 0 || Args.Cgroup != 0)
    {
        return -ENOSYS;
    }

    UINT64 Stack;

    if (Args.Stack == 0 || Args.StackSize == 0 || __builtin_add_overflow(Args.Stack, Args.StackSize, &Stack))
    {
        return -EINVAL;
    }

    return CloneThread(Parent, Frame, &Args, Stack);
}

INT64 SyscallGetTID(VOID)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Idle)
    {
        return -ESRCH;
    }

    return Task->TID;
}

INT64 SyscallFutex(UINT64 Address, UINT64 Operation, UINT64 Value, UINT64 Timeout, UINT64 Address2, UINT64 Value3)
{
    TASK *Task = LBPWRRGetCurrentTask();

    (VOID) Address2;
    (VOID) Value3;

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (Operation != (FUTEX_PRIVATE_FLAG | FUTEX_WAIT) && Operation != (FUTEX_PRIVATE_FLAG | FUTEX_WAKE))
    {
        return -ENOSYS;
    }

    if ((Address & (sizeof(UINT32) - 1)) != 0)
    {
        return -EINVAL;
    }

    UINT64 Deadline = 0;

    if (Operation == (FUTEX_PRIVATE_FLAG | FUTEX_WAIT) && Timeout != 0)
    {
        TIME_SPEC Duration;

        if (!VMMCopyFromUser(Task->Process->AddressSpace, &Duration, Timeout, sizeof(Duration)))
        {
            return -EFAULT;
        }

        if (Duration.Seconds < 0 || Duration.Nanoseconds < 0 || Duration.Nanoseconds >= 1000000000)
        {
            return -EINVAL;
        }

        UINT64 Frequency = TSCGetFrequency();
        UINT64 Fraction  = ((UINT64) Duration.Nanoseconds * Frequency + 999999999) / 1000000000;
        UINT64 Ticks     = (UINT64) Duration.Seconds > (INT64_MAX - Fraction) / Frequency ? INT64_MAX : (UINT64) Duration.Seconds * Frequency + Fraction;
        Deadline         = RDTSC() + Ticks;

        if (Deadline == 0)
        {
            Deadline = 1;
        }
    }

    if (Operation == (FUTEX_PRIVATE_FLAG | FUTEX_WAKE) && (INT32) Value < 0)
    {
        return -EINVAL;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&FutexLock);
    UINT32 Word;
    INT64  Result;

    if (!VMMReadUserWord(Task->Process->AddressSpace, Address, &Word))
    {
        Result = -EFAULT;
    }
    else if (Operation == (FUTEX_PRIVATE_FLAG | FUTEX_WAKE))
    {
        FUTEX_KEY Key = {Task->Process, Address};
        Result        = WaitQueueWakeMatching(&FutexWaiters, (UINT32) Value, FutexMatch, &Key);
    }
    else if (Word != (UINT32) Value)
    {
        Result = -EAGAIN;
    }
    else
    {
        Task->FutexAddress = Address;
        BOOLEAN TimedOut   = FALSE;

        if (Deadline != 0 && (INT64) (RDTSC() - Deadline) >= 0)
        {
            Result = -ETIMEDOUT;
        }
        else if (!WaitQueueSleepTimed(&FutexWaiters, &FutexLock, Deadline, &TimedOut))
        {
            Result = -EINTR;
        }
        else
        {
            Result = TimedOut ? -ETIMEDOUT : 0;
        }
        Task->FutexAddress = 0;
    }

    SpinLockReleaseIRQRestore(&FutexLock, Flags);

    return Result;
}

INT64 SyscallSetTIDAddress(UINT64 Address)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Idle)
    {
        return -ESRCH;
    }

    Task->ClearTIDAddress = Address;

    return Task->TID;
}

VOID SyscallClearTIDAddress(TASK *Task)
{
    UINT64 Address = Task->ClearTIDAddress;

    Task->ClearTIDAddress = 0;

    if (Address != 0)
    {
        UINT64    Flags = SpinLockAcquireIRQSave(&FutexLock);
        FUTEX_KEY Key   = {Task->Process, Address};

        if (VMMWriteUserWord(Task->Process->AddressSpace, Address, 0))
        {
            WaitQueueWakeMatching(&FutexWaiters, 1, FutexMatch, &Key);
        }

        SpinLockReleaseIRQRestore(&FutexLock, Flags);
    }
}

INT64 SyscallArchPrctl(UINT64 Code, UINT64 Address)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    switch (Code)
    {
        case ARCH_SET_FS:
        {
            if (Address >= (1ULL << 47))
            {
                return -EPERM;
            }

            WriteMSR(IA32_FS_BASE, Address);

            return 0;
        }

        case ARCH_GET_FS:
        {
            UINT64 Base = ReadMSR(IA32_FS_BASE);

            return VMMCopyToUser(Task->Process->AddressSpace, Address, &Base, sizeof(Base)) ? 0 : -EFAULT;
        }

        default:
            return -ENOSYS;
    }
}

#define ROBUST_LIST_LIMIT 2048U
#define FUTEX_TID_MASK    0x3FFFFFFFU
#define FUTEX_OWNER_DIED  0x40000000U
#define FUTEX_WAITERS     0x80000000U

typedef struct ROBUST_LIST_HEAD
{
    UINT64 Next;
    INT64  Offset;
    UINT64 Pending;
} ROBUST_LIST_HEAD;

INT64 SyscallSetRobustList(UINT64 Head, UINT64 Length)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Length != sizeof(ROBUST_LIST_HEAD))
    {
        return -EINVAL;
    }

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    Task->RobustListHead = Head;

    return 0;
}

INT64 SyscallGetRobustList(UINT64 TID, UINT64 HeadAddress, UINT64 LengthAddress)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (TID != 0 && TID != Task->TID)
    {
        return -ENOSYS;
    }

    UINT64 Length = sizeof(ROBUST_LIST_HEAD);

    if (!VMMCopyToUser(Task->Process->AddressSpace, LengthAddress, &Length, sizeof(Length)) || !VMMCopyToUser(Task->Process->AddressSpace, HeadAddress, &Task->RobustListHead, sizeof(Task->RobustListHead)))
    {
        return -EFAULT;
    }

    return 0;
}

STATIC BOOLEAN FutexReleaseOwner(TASK *Task, UINT64 Entry, INT64 Offset, BOOLEAN Pending)
{
    if ((Entry & 1U) != 0)
    {
        return TRUE;
    }

    UINT64 Address;
    UINT32 Word;

    if (Offset < 0 ? __builtin_sub_overflow(Entry, 0ULL - (UINT64) Offset, &Address) : __builtin_add_overflow(Entry, (UINT64) Offset, &Address))
    {
        return FALSE;
    }

    if ((Address & (sizeof(Word) - 1)) != 0 || !VMMReadUserWord(Task->Process->AddressSpace, Address, &Word))
    {
        return FALSE;
    }

    for (UINT32 Attempt = 0; Attempt < 32; Attempt++)
    {
        UINT32 Owner = Word & FUTEX_TID_MASK;

        if (Owner == Task->TID)
        {
            UINT32 Expected = Word;

            if (!VMMCompareExchangeUser32(Task->Process->AddressSpace, Address, &Word, (Expected & FUTEX_WAITERS) | FUTEX_OWNER_DIED))
            {
                if (Expected == Word)
                {
                    return FALSE;
                }

                continue;
            }
        }

        if ((Owner == Task->TID && (Word & FUTEX_WAITERS) != 0) || (Pending && Owner == 0))
        {
            UINT64    Flags = SpinLockAcquireIRQSave(&FutexLock);
            FUTEX_KEY Key   = {Task->Process, Address};

            WaitQueueWakeMatching(&FutexWaiters, 1, FutexMatch, &Key);
            SpinLockReleaseIRQRestore(&FutexLock, Flags);
        }

        return TRUE;
    }

    return FALSE;
}

VOID SyscallReleaseRobustList(TASK *Task)
{
    UINT64           Address = Task->RobustListHead;
    ROBUST_LIST_HEAD Head;

    Task->RobustListHead = 0;

    if (Address == 0 || !VMMCopyFromUser(Task->Process->AddressSpace, &Head, Address, sizeof(Head)))
    {
        return;
    }

    UINT64 Entry = Head.Next;

    for (UINT32 Index = 0; Index < ROBUST_LIST_LIMIT && (Entry & ~1ULL) != Address; Index++)
    {
        UINT64 Next;

        if (!VMMCopyFromUser(Task->Process->AddressSpace, &Next, Entry & ~1ULL, sizeof(Next)))
        {
            break;
        }

        if ((Entry & ~1ULL) != (Head.Pending & ~1ULL) && !FutexReleaseOwner(Task, Entry, Head.Offset, FALSE))
        {
            break;
        }

        Entry = Next;
    }

    if (Head.Pending != 0)
    {
        FutexReleaseOwner(Task, Head.Pending, Head.Offset, TRUE);
    }
}

INT64 SyscallExitGroup(UINT64 Status)
{
    return SyscallProcessExit(LBPWRRGetCurrentTask(), Status, TRUE);
}
