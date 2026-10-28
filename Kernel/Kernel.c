// Kernel/Kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/DeviceFS/DeviceFS.h>
#include <Syscall/Syscall.h>
#include <Exec/ELF64.h>
#include <Render/TrueType/Print.h>
#include <Render/TrueType/TrueTypeEngine.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Multicore/SMP.h>
#include <Input/VirtIOInput.h>
#include <Input/Input.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Scheduler/Usage.h>
#include <Arch/AMD64/CPU.h>
#include <Debug/Panic.h>
#include <LineOS/BootInfo.h>
#include <Interrupt/APIC.h>
#include <Interrupt/IDT.h>
#include <Memory/PMM.h>
#include <Memory/VMM.h>
#include <Timer/HPET.h>
#include <Timer/TSC.h>
#include <Timer/Clock.h>
#include <PCI/PCI.h>
#include <Debug/Debug.h>
#include <Disk/VirtIOBlock.h>
#include <Disk/GPT.h>
#include <Network/VirtIONetwork.h>
#include <Network/Network.h>
#include <FS/EXT2/EXT2.h>
#include <FS/VFS/VFS.h>
#include <FS/VFS/VFSEXT2.h>

STATIC VFS_REGISTRY RootRegistry;
STATIC EXT2_VOLUME  RootVolume;
STATIC VFS_CONTEXT  RootContext;
STATIC UINT32       RootPartition;

STATIC ELF64_EXECUTION InitExecution;

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO     *GPU;
    VIRTIO_BLOCK_DEVICE *BlockDevice;
    VFS_NODE             RootNode;
    VFS_CREDENTIALS      Credentials = {0, 0, TRUE};
    CLOCK_TIME           ClockTime;
    UINT32               DisplayWidth;
    UINT32               DisplayHeight;

    CLI();
    CPUEnableSSE();

    KMemoryInit(BootInfo);
    VMMInit();
    KHeapInit();
    TrueTypeInit();
    PCIInit(BootInfo);

    VirtIOGPUInit();
    GPU = VirtIOGPUGetInfo();

    if (GPU == NULL)
    {
        HLT();
    }

    DisplayWidth = GPU->DisplayInfo.Displays[0].Rect.Width;
    DisplayHeight = GPU->DisplayInfo.Displays[0].Rect.Height;

    VirtIOGPUCreateFrameBuffer(DisplayWidth, DisplayHeight);

    GDTInitCurrentCPU(0);

    IDTInit();
    IDTLoad();

    LAPICInit();
    HPETInit(BootInfo);
    TSCCalibrate();
    TSCDeadlineInit(0x40);

    ClockInit();
    ClockGetTime(&ClockTime);

    SMPInit(BootInfo);
    LBPWRRInit();

    VirtIOBlockInit();
    BlockDevice = VirtIOBlockGetDevice();

    VFSInit(&RootRegistry);
    if (VFSMountEXT2(&RootRegistry, (CONST CHAR8 *) "root", &RootVolume, BlockDevice, 2, TRUE, &RootPartition) != VFS_OK)
    {
        Panic(L"Root Filesystem Mount Failed");
    }
    if (DeviceFSMount(&RootRegistry) != VFS_OK)
    {
        Panic(L"DeviceFS Mount Failed");
    }
    VFSContextInit(&RootContext, &RootRegistry, RootPartition, Credentials);
    VFSLookup(&RootContext, (CONST CHAR8 *) "/", &RootNode);

    VirtIONetworkInit();
    NetworkInit();

    VirtIOInputInit();
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);

    TASK *KeyboardTask = TaskCreate(InputKeyboardHandler);

    if (KeyboardTask == NULL || !LBPWRRAddTask(KeyboardTask))
    {
        Panic(L"Keyboard Task Failed");
    }

    TASK *MouseTask = TaskCreate(InputMouseHandler);

    if (MouseTask == NULL || !LBPWRRAddTask(MouseTask))
    {
        Panic(L"Mouse Task Failed");
    }

    KMemSet(&InitExecution, 0, sizeof(InitExecution));

    if (!ELF64ExecuteInit(&RootContext, (CONST CHAR8 *) "/sbin/Init.lix", &InitExecution))
    {
        Panic(L"Init Failed");
    }

    LBPWRRStart();

    for (;;)
    {
        STIHLT();
    }
}

VOID APMain(UINT32 CPUID)
{
    CPU_INFO *CPU;

    CLI();

    CPU = SMPGetCPU(CPUID);

    GDTInitCurrentCPU(CPUID);
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
        ATOMIC_STORE(&CPU->Online, TRUE, ATOMIC_RELEASE);
    }

    CompilerBarrier();

    for (;;)
    {
        LBPWRRJoin();
        STIHLT();
    }
}