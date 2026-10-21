// Kernel/Kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/TrueType/Print.h>
#include <Render/TrueType/TrueTypeEngine.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Multicore/SMP.h>
#include <Input/VirtIOInput.h>
#include <Input/Input.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Arch/AMD64/CPU.h>
#include <Debug/Panic.h>
#include <LineOS/BootInfo.h>
#include <Interrupt/APIC.h>
#include <Interrupt/IDT.h>
#include <Memory/PMM.h>
#include <Timer/HPET.h>
#include <Timer/TSC.h>
#include <Timer/Clock.h>
#include <PCI/PCI.h>
#include <Debug/Debug.h>
#include <Memory/VMM.h>
#include <Disk/VirtIOBlock.h>
#include <Network/VirtIONetwork.h>
#include <Network/Network.h>
#include <FS/EXT2/EXT2.h>
#include <FS/VFS/VFSEXT2.h>

VOID MouseTask(VOID)
{
    InputMouseHandler();
}

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();
    CPUEnableSSE();

    KMemoryInit(BootInfo);

    VMMInit();
    KHeapInit();

    TrueTypeInit();
    PCIInit(BootInfo);

    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    GDTInitCurrentCPU();

    IDTInit();
    IDTLoad();

    LAPICInit();
    HPETInit(BootInfo);
    TSCCalibrate();
    TSCDeadlineInit(0x40);

    ClockInit();

    SMPInit(BootInfo);

    LBPWRRInit();

    VirtIOBlockInit();
    VirtIONetworkInit();
    NetworkInit();
    VirtIOInputInit();
}

VOID ClockDisplayTask(VOID)
{
    for (;;)
    {
        CLOCK_DATE_TIME  DateTime;
        CLOCK_NTP_STATUS Status;
        UINT64           Uptime = ClockGetUptimeMls() / 1000;
        UINT32           Width = VirtIOGPUGetFrameBufferWidth();
        UINT32           PixelHeight = Width >= 1920 ? 72 : Width >= 1280 ? 48 : 32;
        ClockGetNTPStatus(&Status);

        if (Width > 100)
        {
            FillRect(64, 120, Width - 80, 300, 0x1E1E1E);
            KPrint(L"Uptime: %llu days %02llu:%02llu:%02llu", 80, 190, 0xCCCCCC, PixelHeight - 8, JETBRAINS_MONO, Uptime / 86400, (Uptime / 3600) % 24, (Uptime / 60) % 60, Uptime % 60);

            if (ClockGetDateTime(&DateTime, 540))
            {
                KPrint(L"KST: %04u-%02u-%02u %02u:%02u:%02u (UTC+09:00)", 80, 285, 0xFFFFFF, PixelHeight, JETBRAINS_MONO, (UINT32) DateTime.Year, (UINT32) DateTime.Month, (UINT32) DateTime.Day, (UINT32) DateTime.Hour, (UINT32) DateTime.Minute, (UINT32) DateTime.Second);
            }
            else
            {
                KPrint(L"Current time unavailable", 80, 285, 0xE5C07B, PixelHeight, JETBRAINS_MONO);
            }

            CONST CHAR16 *State = Status.Busy ? L"synchronizing" : Status.Failures != 0 ? (Status.Synchronized ? L"retry (last sync)" : L"retry pending (RTC)") : Status.Synchronized ? L"synchronized" : L"waiting (RTC)";
            KPrint(L"NTP: %s / interval 10 min", 80, 380, (Status.Synchronized && Status.Failures == 0) ? 0x98C379 : 0xE5C07B, PixelHeight - 16, JETBRAINS_MONO, State);
            VirtIOGPUFlush();
        }
        TaskSleepMls(100);
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{

    InitKernel(BootInfo);

    UINT64 FrameRIP;
    UINT64 FrameCS;
    UINT64 FrameRFLAGS;

    ASM("movw %%cs, %%ax\n\t"
        "movzwq %%ax, %%rax\n\t"
        "leaq 1f(%%rip), %%rcx\n\t"

        "pushfq\n\t"
        "pushq %%rax\n\t"
        "pushq %%rcx\n\t"

        "movq (%%rsp), %0\n\t"
        "movq 8(%%rsp), %1\n\t"
        "movq 16(%%rsp), %2\n\t"

        "addq $24, %%rsp\n\t"
        "1:\n\t"

        : "=&r"(FrameRIP), "=&r"(FrameCS), "=&r"(FrameRFLAGS)
        :
        : "rax", "rcx", "memory", "cc");

    FillScreen(0x1E1E1E);

    // KPrint(L"%016llX, %016llX, %016llX", 100, 100, 0xFFFFFF, 28, JETBRAINS_MONO, FrameRIP, FrameCS, FrameRFLAGS);

    VirtIOGPUFlush();

    if (NetworkInit())
    {
        TASK *NetworkWorker = TaskCreate(NetworkTask);

        if (NetworkWorker != NULL)
        {
            if (!LBPWRRAddTask(NetworkWorker))
            {
                TaskDestroy(NetworkWorker);
            }
        }
    }

    TASK *CursorTask = TaskCreate(MouseTask);
    TASK *ClockWorker = TaskCreate(ClockNTPTask);
    TASK *ClockDisplay = TaskCreate(ClockDisplayTask);

    if (ClockWorker != NULL && !LBPWRRAddTask(ClockWorker))
    {
        TaskDestroy(ClockWorker);
    }

    if (ClockDisplay != NULL && !LBPWRRAddTask(ClockDisplay))
    {
        TaskDestroy(ClockDisplay);
    }

    LBPWRRAddTask(CursorTask);

    LBPWRRStart();

    HLT();
}

VOID APMain(UINT32 CPUID)
{
    CPU_INFO *CPU;

    CLI();

    CPU = SMPGetCPU(CPUID);

    GDTInitCurrentCPU();
    CPUEnableSSE();
    IDTLoad();

    if (!LAPICInitCurrentCPU())
    {
        HLT();
    }

    if (!TSCDeadlineInit(0x40))
    {
        HLT();
    }

    if (CPU != NULL)
    {
        __atomic_store_n(&CPU->Online, TRUE, __ATOMIC_RELEASE);
    }

    CompilerBarrier();

    for (;;)
    {
        LBPWRRJoin();
        STIHLT();
    }
}
