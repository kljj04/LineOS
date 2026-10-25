// Kernel/Source/Syscall/Syscall.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Interrupt/IDT.h>
#include <Syscall/Syscall.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/TaskTypes.h>
#include <Syscall/SyscallExit.h>
#include <Syscall/SyscallWrite.h>
#include <Syscall/SyscallDisplay.h>

UINT64 SyscallDispatch(INTERRUPT_FRAME *Frame)
{
    if (Frame == NULL)
    {
        return (UINT64) -1;
    }

    switch (Frame->RAX)
    {
        case SYSCALL_EXIT:
        {
            return (UINT64) SyscallExit(Frame->RDI);
        }

        case SYSCALL_READ:
        case SYSCALL_OPEN:
        case SYSCALL_CLOSE:
        case SYSCALL_YIELD:
        {
            return (UINT64) -1;
        }

        case SYSCALL_WRITE:
        {
            return (UINT64) SyscallWrite(Frame->RDI, (CONST CHAR8 *) Frame->RSI, Frame->RDX);
        }

        case SYSCALL_GETPID:
        {
            TASK *Task = LBPWRRGetCurrentTask();

            if (Task == NULL)
            {
                return (UINT64) -1;
            }

            return Task->PID;
        }

        case SYSCALL_DISPLAY_INFO:
        {
            return (UINT64) SyscallDisplayInfo(Frame->RDI);
        }

        case SYSCALL_DISPLAY_PRESENT:
        {
            return (UINT64) SyscallDisplayPresent(Frame->RDI, Frame->RSI);
        }

        case SYSCALL_DISPLAY_PRESENT_RECT:
        {
            return (UINT64) SyscallDisplayPresentRect(Frame->RDI);
        }

        default:
        {
            return (UINT64) -1;
        }
    }
}