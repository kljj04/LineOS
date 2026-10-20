// Kernel/Kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/TrueType/Print.h>
#include <Render/Web/Web.h>
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

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{

    InitKernel(BootInfo);

    UINT64 FrameRIP;
    UINT64 FrameCS;
    UINT64 FrameRFLAGS;

    ASM(
        "movw %%cs, %%ax\n\t"
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

        : "=&r"(FrameRIP),
          "=&r"(FrameCS),
          "=&r"(FrameRFLAGS)
        :
        : "rax", "rcx", "memory", "cc"
    );

    FillScreen(0x1E1E1E);

    KPrint(L"%016llX, %016llX, %016llX", 100, 100, 0xFFFFFF,28,JETBRAINS_MONO, FrameRIP, FrameCS, FrameRFLAGS);

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

    TASK *BrowserWorker = TaskCreate(WebTask);
    TASK *KeyboardWorker = TaskCreate(InputKeyboardHandler);
    TASK *MouseWorker = TaskCreate(InputMouseHandler);

    if (BrowserWorker == NULL || KeyboardWorker == NULL || MouseWorker == NULL || !LBPWRRAddTask(BrowserWorker) || !LBPWRRAddTask(KeyboardWorker) || !LBPWRRAddTask(MouseWorker))
    {
        Panic(L"Web task creation failed");
    }

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
