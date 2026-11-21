// Kernel/Source/Syscall/Syscall.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Interrupt/IDT.h>
#include <Debug/Debug.h>
#include <Syscall/File/SyscallDirectory.h>
#include <Syscall/File/SyscallStat.h>
#include <Syscall/File/SyscallPath.h>
#include <Syscall/File/SyscallOwnership.h>
#include <Syscall/Time/SyscallFileTime.h>
#include <Syscall/Device/SyscallIoctl.h>
#include <Syscall/System/SyscallRandom.h>
#include <Syscall/Process/SyscallIdentity.h>
#include <Syscall/Security/SyscallParentChdir.h>
#include <Syscall/Syscall.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/TaskTypes.h>
#include <Syscall/Process/SyscallExit.h>
#include <Syscall/Device/SyscallDisplay.h>
#include <Syscall/File/SyscallFile.h>
#include <Syscall/Process/SyscallProcess.h>
#include <Syscall/Process/SyscallThread.h>
#include <Syscall/Asset/SyscallAsset.h>
#include <Syscall/Memory/SyscallMemory.h>
#include <Syscall/System/SyscallSysinfo.h>
#include <Syscall/Time/SyscallTime.h>
#include <Syscall/Security/SyscallSudo.h>
#include <Syscall/System/SyscallMonitor.h>
#include <Syscall/IPC/SyscallIPC.h>
#include <Syscall/System/SyscallAffinity.h>
#include <Syscall/Network/SyscallSocket.h>

#include <Arch/AMD64/CPU.h>
#include <Debug/Panic.h>
#include <Syscall/Errno.h>

STATIC_ASSERT(GDT_KERNEL_DATA_SELECTOR == GDT_KERNEL_CODE_SELECTOR + 8, "SYSCALL kernel selectors");
STATIC_ASSERT(GDT_USER_DATA_SELECTOR == GDT_USER_CODE_SELECTOR - 8, "SYSRET user selectors");
STATIC_ASSERT(GDT_USER_DATA_SELECTOR == 0x18 && GDT_USER_CODE_SELECTOR == 0x20, "SyscallEntry selectors");
STATIC_ASSERT(OFFSET_OF(SYSCALL_CPU, KernelStack) == 0 && OFFSET_OF(SYSCALL_CPU, UserStack) == 8, "SyscallEntry CPU offsets");
STATIC_ASSERT(OFFSET_OF(INTERRUPT_FRAME, RAX) == 112 && OFFSET_OF(INTERRUPT_FRAME, RIP) == 136 && OFFSET_OF(INTERRUPT_FRAME, RFLAGS) == 152 && OFFSET_OF(INTERRUPT_FRAME, RSP) == 160 && sizeof(INTERRUPT_FRAME) == 176, "SyscallEntry frame layout");

STATIC INT64 SyscallMembarrier(SYSCALL_CONTEXT *Context)
{
    return Context->Args.RDI == 0 && Context->Args.RSI == 0 ? 0 : -EINVAL;
}

STATIC INT64 SyscallYield(SYSCALL_CONTEXT *Context)
{
    (VOID) Context;
    LBPWRRYield();

    return 0;
}

STATIC INT64 SyscallGetPID(SYSCALL_CONTEXT *Context)
{
    (VOID) Context;
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        return -ESRCH;
    }

    return Task->Process->PID;
}

STATIC CONST SYSCALL_ENTRY SyscallTable[] = {
    {SYSCALL_READ,                 SyscallRead              },
    {SYSCALL_WRITE,                SyscallFileWrite         },
    {SYSCALL_OPEN,                 SyscallOpen              },
    {SYSCALL_CLOSE,                SyscallClose             },
    {SYSCALL_STAT,                 SyscallStat              },
    {SYSCALL_FSTAT,                SyscallFstat             },
    {SYSCALL_LSTAT,                SyscallStat              },
    {SYSCALL_POLL,                 SyscallPoll              },
    {SYSCALL_MMAP,                 SyscallMmap              },
    {SYSCALL_MPROTECT,             SyscallMprotect          },
    {SYSCALL_MUNMAP,               SyscallMunmap            },
    {SYSCALL_BRK,                  SyscallBrk               },
    {SYSCALL_RT_SIGPROCMASK,       SyscallRTSigprocmask     },
    {SYSCALL_IOCTL,                SyscallIoctl             },
    {SYSCALL_PREAD64,              SyscallPread64           },
    {SYSCALL_WRITEV,               SyscallFileWritev        },
    {SYSCALL_ACCESS,               SyscallAccess            },
    {SYSCALL_YIELD,                SyscallYield             },
    {SYSCALL_NANOSLEEP,            SyscallNanosleep         },
    {SYSCALL_GETPID,               SyscallGetPID            },
    {SYSCALL_SOCKET,               SyscallSocket            },
    {SYSCALL_CONNECT,              SyscallConnect           },
    {SYSCALL_ACCEPT,               SyscallAccept            },
    {SYSCALL_ACCEPT4,              SyscallAccept            },
    {SYSCALL_SENDTO,               SyscallSendTo            },
    {SYSCALL_RECVFROM,             SyscallRecvFrom          },
    {SYSCALL_SHUTDOWN,             SyscallShutdown          },
    {SYSCALL_BIND,                 SyscallBind              },
    {SYSCALL_LISTEN,               SyscallListen            },
    {SYSCALL_GETSOCKNAME,          SyscallSocketName        },
    {SYSCALL_GETPEERNAME,          SyscallSocketName        },
    {SYSCALL_SETSOCKOPT,           SyscallSocketOption      },
    {SYSCALL_GETSOCKOPT,           SyscallSocketOption      },
    {SYSCALL_CLONE,                SyscallClone             },
    {SYSCALL_FORK,                 SyscallFork              },
    {SYSCALL_EXECVE,               SyscallExecVE            },
    {SYSCALL_EXIT,                 SyscallExit              },
    {SYSCALL_WAIT4,                SyscallWait4             },
    {SYSCALL_FCNTL,                SyscallFcntl             },
    {SYSCALL_GETCWD,               SyscallGetCwd            },
    {SYSCALL_CHDIR,                SyscallChdir             },
    {SYSCALL_RENAME,               SyscallRename            },
    {SYSCALL_MKDIR,                SyscallMkdir             },
    {SYSCALL_CHOWN,                SyscallChown             },
    {SYSCALL_SYSINFO,              SyscallSysInfo           },
    {SYSCALL_GETUID,               SyscallGetUID            },
    {SYSCALL_SETUID,               SyscallSetUID            },
    {SYSCALL_SETGID,               SyscallSetGID            },
    {SYSCALL_GETEUID,              SyscallGetUID            },
    {SYSCALL_ARCH_PRCTL,           SyscallArchPrctl         },
    {SYSCALL_GETTID,               SyscallGetTID            },
    {SYSCALL_FUTEX,                SyscallFutex             },
    {SYSCALL_AFFINITY,             SyscallSchedGetAffinity  },
    {SYSCALL_GETDENTS64,           SyscallGetDents64        },
    {SYSCALL_SET_TID_ADDRESS,      SyscallSetTIDAddress     },
    {SYSCALL_CLOCK_GETTIME,        SyscallClockGetTime      },
    {SYSCALL_EXIT_GROUP,           SyscallExitGroup         },
    {SYSCALL_OPENAT,               SyscallOpenat            },
    {SYSCALL_NEWFSTATAT,           SyscallNewfstatat        },
    {SYSCALL_SET_ROBUST_LIST,      SyscallSetRobustList     },
    {SYSCALL_GET_ROBUST_LIST,      SyscallGetRobustList     },
    {SYSCALL_UTIMENSAT,            SyscallUtimensat         },
    {SYSCALL_GETRANDOM,            SyscallGetRandom         },
    {SYSCALL_MEMBARRIER,           SyscallMembarrier        },
    {SYSCALL_CLONE3,               SyscallClone3            },

    {SYSCALL_DISPLAY_INFO,         SyscallDisplayInfo       },
    {SYSCALL_DISPLAY_PRESENT,      SyscallDisplayPresent    },
    {SYSCALL_DISPLAY_PRESENT_RECT, SyscallDisplayPresentRect},
    {SYSCALL_ASSET_PUBLISH,        SyscallAssetPublish      },
    {SYSCALL_ASSET_ACQUIRE,        SyscallAssetAcquire      },
    {SYSCALL_ASSET_RELEASE,        SyscallAssetRelease      },
    {SYSCALL_REMOVE,               SyscallRemove            },
    {SYSCALL_PARENT_CHDIR,         SyscallParentChdir       },
    {SYSCALL_SUDO_CACHE,           SyscallSudoCache         },
    {SYSCALL_SYSTEM_MONITOR,       SyscallSystemMonitor     },
    {SYSCALL_IPC,                  SyscallIPC               }
};

STATIC SYSCALL_CPU SyscallCPUs[GDT_MAX_CPUS];

EXTERN VOID SyscallEntry(VOID);

STATIC SYSCALL_ABI_ARGS SyscallGetABIArgs(INTERRUPT_FRAME *Frame)
{
    return (SYSCALL_ABI_ARGS) {.RDI = Frame->RDI, .RSI = Frame->RSI, .RDX = Frame->RDX, .R10 = Frame->R10, .R8 = Frame->R8, .R9 = Frame->R9};
}

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

    SYSCALL_CONTEXT Context = {.Number = Frame->RAX, .Args = SyscallGetABIArgs(Frame), .Frame = Frame};

    for (UINT64 Index = 0; Index < sizeof(SyscallTable) / sizeof(SyscallTable[0]); Index++)
    {
        if (SyscallTable[Index].Number == Context.Number)
        {
            return (UINT64) SyscallTable[Index].Handler(&Context);
        }
    }

    DebugWrite("Unhandled syscall: ");
    DebugWriteDec(Context.Number);
    DebugWrite("\n");

    return (UINT64) -ENOSYS;
}
