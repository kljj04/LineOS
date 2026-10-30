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
#include <Syscall/SyscallProcess.h>
#include <Syscall/SyscallAsset.h>
#include <Syscall/SyscallMemory.h>
#include <Syscall/SyscallSysinfo.h>
#include <Syscall/SyscallTime.h>

#include <Arch/AMD64/CPU.h>
#include <Debug/Panic.h>
#include <LineOS/Errno.h>

typedef struct
{
    UINT64 KernelStack;
    UINT64 UserStack;
} SYSCALL_CPU;

STATIC_ASSERT(GDT_KERNEL_DATA_SELECTOR == GDT_KERNEL_CODE_SELECTOR + 8, "SYSCALL kernel selectors");
STATIC_ASSERT(GDT_USER_DATA_SELECTOR == GDT_USER_CODE_SELECTOR - 8, "SYSRET user selectors");
STATIC_ASSERT(GDT_USER_DATA_SELECTOR == 0x18 && GDT_USER_CODE_SELECTOR == 0x20, "SyscallEntry selectors");
STATIC_ASSERT(OFFSET_OF(SYSCALL_CPU, KernelStack) == 0 && OFFSET_OF(SYSCALL_CPU, UserStack) == 8, "SyscallEntry CPU offsets");
STATIC_ASSERT(OFFSET_OF(INTERRUPT_FRAME, RAX) == 112 && OFFSET_OF(INTERRUPT_FRAME, RIP) == 136 && OFFSET_OF(INTERRUPT_FRAME, RFLAGS) == 152 && OFFSET_OF(INTERRUPT_FRAME, RSP) == 160 && sizeof(INTERRUPT_FRAME) == 176, "SyscallEntry frame layout");

STATIC SYSCALL_CPU SyscallCPUs[GDT_MAX_CPUS];

EXTERN VOID SyscallEntry(VOID);

VOID SyscallInitCurrentCPU(UINT32 CPUIndex)
{
    UINT32 EAX;
    UINT32 EBX;
    UINT32 ECX;
    UINT32 EDX;

    CPUID(0x80000000U, 0, &EAX, &EBX, &ECX, &EDX);
    if (CPUIndex >= GDT_MAX_CPUS || EAX < 0x80000001U)
    {
        Panic(L"SYSCALL unavailable");
    }
    CPUID(0x80000001U, 0, &EAX, &EBX, &ECX, &EDX);
    if ((EDX & (1U << 11)) == 0)
    {
        Panic(L"SYSCALL unavailable");
    }

    WriteMSR(0xC0000101U, 0);
    WriteMSR(0xC0000102U, (UINT64) &SyscallCPUs[CPUIndex]);
    WriteMSR(0xC0000081U, ((UINT64) ((GDT_USER_CODE_SELECTOR - 16) | 3) << 48) | ((UINT64) GDT_KERNEL_CODE_SELECTOR << 32));
    WriteMSR(0xC0000082U, (UINT64) SyscallEntry);
    WriteMSR(0xC0000084U, (1ULL << 8) | (1ULL << 9) | (1ULL << 10) | (3ULL << 12) | (1ULL << 14) | (1ULL << 16) | (1ULL << 18));
    WriteMSR(0xC0000080U, ReadMSR(0xC0000080U) | 1ULL);
}

VOID SyscallSetKernelStack(UINT32 CPUIndex, UINT64 StackTop)
{
    SyscallCPUs[CPUIndex].KernelStack = StackTop;
}

UINT64 SyscallDispatch(INTERRUPT_FRAME *Frame)
{
    if (Frame == NULL)
    {
        return (UINT64) -ENOSYS;
    }

    switch (Frame->RAX)
    {
        case SYSCALL_CLOCK_GETTIME:
        {
            return (UINT64) SyscallClockGetTime((INT64) Frame->RDI, Frame->RSI);
        }
        case SYSCALL_GETUID:
        case SYSCALL_GETEUID:
        {
            return (UINT64) SyscallGetUID(Frame->RAX == SYSCALL_GETEUID);
        }
        case SYSCALL_PARENT_CHDIR:
        {
            return (UINT64) SyscallParentChdir(Frame->RDI);
        }
        case SYSCALL_STAT:
        case SYSCALL_LSTAT:
        {
            return (UINT64) SyscallStat(Frame->RDI, Frame->RSI, Frame->RAX == SYSCALL_STAT);
        }
        case SYSCALL_FSTAT:
        {
            return (UINT64) SyscallFstat(Frame->RDI, Frame->RSI);
        }
        case SYSCALL_MMAP:
        {
            return (UINT64) SyscallMmap(Frame->RDI, Frame->RSI, Frame->RDX, Frame->R10, (INT64) Frame->R8, Frame->R9);
        }
        case SYSCALL_MUNMAP:
        {
            return (UINT64) SyscallMunmap(Frame->RDI, Frame->RSI);
        }
        case SYSCALL_FORK:
        {
            return (UINT64) SyscallFork(Frame);
        }
        case SYSCALL_WAIT4:
        {
            return (UINT64) SyscallWait4((INT64) Frame->RDI, Frame->RSI, Frame->RDX, Frame->R10);
        }
        case SYSCALL_EXECVE:
        {
            return (UINT64) SyscallExecVE(Frame);
        }
        case SYSCALL_GETDENTS64:
        {
            return (UINT64) SyscallGetDents64(Frame->RDI, Frame->RSI, Frame->RDX);
        }
        case SYSCALL_IOCTL:
        {
            return (UINT64) SyscallIoctl(Frame->RDI, Frame->RSI, Frame->RDX);
        }

        case SYSCALL_EXIT:
        {
            return (UINT64) SyscallExit(Frame->RDI);
        }

        case SYSCALL_CHOWN:
            return (UINT64) SyscallChown(Frame->RDI, Frame->RSI, Frame->RDX);
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

        case SYSCALL_GETCWD:
        {
            return (UINT64) SyscallGetCwd(Frame->RDI, Frame->RSI);
        }
        case SYSCALL_CHDIR:
        {
            return (UINT64) SyscallChdir(Frame->RDI);
        }
        case SYSCALL_RENAME:
        {
            return (UINT64) SyscallRename(Frame->RDI, Frame->RSI);
        }

        case SYSCALL_UTIMENSAT:
        {
            return (UINT64) SyscallUtimensat((INT64) Frame->RDI, Frame->RSI, Frame->RDX, Frame->R10);
        }

        case SYSCALL_MKDIR:
        {
            return (UINT64) SyscallMkdir(Frame->RDI, Frame->RSI);
        }

        case SYSCALL_SYSINFO:
        {
            return SyscallSysInfo(Frame->RDI);
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
                return (UINT64) -ESRCH;
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
            return (UINT64) -ENOSYS;
        }
    }
}