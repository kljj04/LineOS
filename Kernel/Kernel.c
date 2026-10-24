// Kernel/Kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "Scheduler/LBPWRRRunQueue.h"

#include <stdio.h>
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
#include <Timer/HPET.h>
#include <Timer/TSC.h>
#include <Timer/Clock.h>
#include <PCI/PCI.h>
#include <Debug/Debug.h>
#include <Memory/VMM.h>
#include <Disk/VirtIOBlock.h>
#include <Disk/GPT.h>
#include <Network/VirtIONetwork.h>
#include <Network/Network.h>
#include <FS/EXT2/EXT2.h>
#include <FS/VFS/VFS.h>
#include <FS/VFS/VFSEXT2.h>
#include <Exec/ELF64.h>

#define KERNEL_ELF_MAX_SIZE (64ULL * 1024 * 1024)
#define KERNEL_ELF_READ_SIZE 4096

STATIC VFS_REGISTRY RootRegistry;
STATIC EXT2_VOLUME RootVolume;
STATIC VFS_CONTEXT RootContext;
STATIC UINT32 RootPartition;

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;
    VIRTIO_BLOCK_DEVICE *BlockDevice;
    VIRTIO_NETWORK_DEVICE *NetworkDevice;
    VFS_NODE RootNode;
    VFS_CREDENTIALS Credentials = {0, 0, TRUE};
    CLOCK_TIME ClockTime;
    UINT32 DisplayWidth;
    UINT32 DisplayHeight;

    CLI();
    CPUEnableSSE();

    KMemoryInit(BootInfo);
    VMMInit();
    KHeapInit();
    TrueTypeInit();
    PCIInit(BootInfo);

    GPU = NULL;

    VirtIOGPUInit();
    GPU = VirtIOGPUGetInfo();

    DisplayWidth = GPU->DisplayInfo.Displays[0].Rect.Width;
    DisplayHeight = GPU->DisplayInfo.Displays[0].Rect.Height;

    VirtIOGPUCreateFrameBuffer(DisplayWidth, DisplayHeight);

    GDTInitCurrentCPU();

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
    VFSMountEXT2(&RootRegistry, (CONST CHAR8 *) "root", &RootVolume, BlockDevice, 2, FALSE, &RootPartition);
    VFSContextInit(&RootContext, &RootRegistry, RootPartition, Credentials);
    VFSLookup(&RootContext, (CONST CHAR8 *) "/", &RootNode);

    VirtIONetworkInit();
    NetworkDevice = VirtIONetworkGetDevice();
    NetworkInit();

    VirtIOInputInit();
}

STATIC BOOLEAN KernelValidateInitELF(VOID)
{
    VFS_NODE Node;
    VOID *FileData;
    UINT64 Offset;
    UINT32 Chunk;
    UINT32 ReadLength;
    BOOLEAN Valid;

    if (VFSLookup(&RootContext, (CONST CHAR8 *) "/bin/init.lix", &Node) != VFS_OK)
    {
        DebugWriteLine("ELF64: /bin/init not found");
        return FALSE;
    }

    if ((Node.Mode & VFS_MODE_TYPE) != EXT2_MODE_FILE || Node.Size < 64 || Node.Size > KERNEL_ELF_MAX_SIZE)
    {
        DebugWriteLine("ELF64: invalid executable file");
        return FALSE;
    }

    FileData = KAlloc((UINTN) Node.Size);

    if (FileData == NULL)
    {
        DebugWriteLine("ELF64: allocation failed");
        return FALSE;
    }

    for (Offset = 0; Offset < Node.Size;)
    {
        Chunk = Node.Size - Offset > KERNEL_ELF_READ_SIZE ? KERNEL_ELF_READ_SIZE : (UINT32) (Node.Size - Offset);
        ReadLength = 0;

        if (VFSRead(&RootContext, (CONST CHAR8 *) "/bin/init.lix", Offset, (UINT8 *) FileData + Offset, Chunk, &ReadLength) != VFS_OK || ReadLength != Chunk)
        {
            KFree(FileData);
            DebugWriteLine("ELF64: file read failed");
            return FALSE;
        }

        Offset += Chunk;
    }

    Valid = ELF64Validate(FileData, Node.Size);

    KFree(FileData);

    if (!Valid)
    {
        DebugWriteLine("ELF64: validation failed");
        return FALSE;
    }

    DebugWriteLine("ELF64: /bin/init validated");

    return TRUE;
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);




    if (!KernelValidateInitELF())
    {
        Panic(L"Failed to validate /bin/init");
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
        ATOMIC_STORE(&CPU->Online, TRUE, ATOMIC_RELEASE);
    }

    CompilerBarrier();

    for (;;)
    {
        LBPWRRJoin();
        STIHLT();
    }
}