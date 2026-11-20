// Kernel/Source/Syscall/System/SyscallSysinfo.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LineOS/TypeInfo.h>
#include <Syscall/Errno.h>
#include <Syscall/System/SyscallSysinfo.h>
#include <Syscall/System/Sysinfo.h>
#include <Timer/Clock.h>
#include <Memory/VMM.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Memory/PMM.h>

INT64 SyscallSysInfo(SYSCALL_CONTEXT *Context)
{
    UINT64 UserInfo = Context->Args.RDI;

    TASK    *Task;
    SYS_INFO Info = {0};

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    Info.Uptime = (INT64) (ClockGetUptimeMls() / 1000);

    UINT64 Managed;
    UINT64 Free;

    KGetManagedMemory(&Managed, &Free);
    Info.TotalRAM = Managed;
    Info.FreeRAM  = Free;
    Info.MemUnit  = 1;

    UINTN Count = TaskGetCount();

    Info.Processes = Count > 65535U ? 65535U : (UINT16) Count;

    if (!VMMCopyToUser(Task->Process->AddressSpace, UserInfo, &Info, sizeof(Info)))
    {
        return -EFAULT;
    }

    return 0;
}