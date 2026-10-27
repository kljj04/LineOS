// Kernel/Source/Syscall/Syscall.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Interrupt/IDT.h>
#include <Syscall/Syscall.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/TaskTypes.h>
#include <Syscall/SyscallExit.h>
#include <Syscall/SyscallDisplay.h>
#include <Syscall/SyscallFile.h>
#include <Syscall/SyscallSpawn.h>
#include <Syscall/SyscallAsset.h>

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

        case SYSCALL_SETUID:
        {
            return (UINT64) SyscallSetUID(Frame->RDI);
        }

        case SYSCALL_SETGID:
        {
            return (UINT64) SyscallSetGID(Frame->RDI);
        }

        case SYSCALL_GETRANDOM:
        {
            return (UINT64) SyscallGetRandom(Frame->RDI, Frame->RSI, Frame->RDX);
        }

        case SYSCALL_READ:
        {
            return (UINT64) SyscallRead(Frame->RDI, Frame->RSI, Frame->RDX);
        }

        case SYSCALL_OPEN:
        {
            return (UINT64) SyscallOpen(Frame->RDI, Frame->RSI, Frame->RDX);
        }

        case SYSCALL_RENAME:
        {
            return (UINT64) SyscallRename(Frame->RDI, Frame->RSI);
        }

        case SYSCALL_MKDIR:
        {
            return (UINT64) SyscallMkdir(Frame->RDI, Frame->RSI);
        }

        case SYSCALL_REMOVE:
        {
            return (UINT64) SyscallRemove(Frame->RDI);
        }

        case SYSCALL_CLOSE:
        {
            return (UINT64) SyscallClose(Frame->RDI);
        }

        case SYSCALL_YIELD:
        {
            LBPWRRYield();

            return 0;
        }

        case SYSCALL_WRITE:
        {
            return (UINT64) SyscallFileWrite(Frame->RDI, Frame->RSI, Frame->RDX);
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

        case SYSCALL_SPAWN:
        {
            return (UINT64) SyscallSpawn(Frame->RDI);
        }

        case SYSCALL_SPAWNV:
        {
            return (UINT64) SyscallSpawnV(Frame->RDI, Frame->RSI);
        }

        case SYSCALL_ASSET_PUBLISH:
        {
            return (UINT64) SyscallAssetPublish(Frame->RDI, Frame->RSI, Frame->RDX);
        }

        case SYSCALL_ASSET_ACQUIRE:
        {
            return (UINT64) SyscallAssetAcquire(Frame->RDI, Frame->RSI);
        }

        case SYSCALL_ASSET_RELEASE:
        {
            return (UINT64) SyscallAssetRelease(Frame->RDI);
        }

        default:
        {
            return (UINT64) -1;
        }
    }
}