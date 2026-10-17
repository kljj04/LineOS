// kernel/kernel.c
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
#include <PCI/PCI.h>
#include <Debug/Debug.h>
#include <Memory/VMM.h>
#include <Disk/VirtIOBlock.h>

STATIC VOID InputKeyboardTask(VOID)
{
    InputKeyboardHandler();
}

STATIC VOID InputMouseTask(VOID)
{
    InputMouseHandler();
}

#define INPUT_TEST_INTERVAL_MS 8

#define INPUT_PANEL_X      20
#define INPUT_PANEL_Y      200
#define INPUT_PANEL_WIDTH  900
#define INPUT_PANEL_HEIGHT 420

STATIC VOID InputTestTask(VOID)
{
    KEYBOARD_STATUS Keyboard;
    KEYBOARD_STATUS LastKeyboard = {0};
    MOUSE_STATUS Mouse;
    MOUSE_STATUS LastMouse = {0};

    BOOLEAN FirstFrame = TRUE;
    BOOLEAN KeyboardChanged;
    BOOLEAN MouseChanged;
    BOOLEAN ButtonsChanged;

    while (TRUE)
    {
        Keyboard = GetKeyboardStatus();
        Mouse = GetMouseStatus();

        KeyboardChanged = FirstFrame || Keyboard.KeyCode != LastKeyboard.KeyCode || Keyboard.Pressed != LastKeyboard.Pressed;

        MouseChanged = FirstFrame || Mouse.X != LastMouse.X || Mouse.Y != LastMouse.Y;

        ButtonsChanged = FirstFrame || Mouse.LeftButton != LastMouse.LeftButton || Mouse.RightButton != LastMouse.RightButton || Mouse.MiddleButton != LastMouse.MiddleButton || Mouse.Wheel != LastMouse.Wheel || Mouse.BackButton != LastMouse.BackButton || Mouse.ForwardButton != LastMouse.ForwardButton;

        if (KeyboardChanged || MouseChanged || ButtonsChanged)
        {
            FillRect(INPUT_PANEL_X, INPUT_PANEL_Y, INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT, 0x101010);

            KPrint(L"KEYBOARD", 40, 240, 0x80C0FF, 32, PRETENDARD);
            KPrint(L"Key Code: 0x%X", 40, 290, 0xFFFFFF, 26, JETBRAINS_MONO, Keyboard.KeyCode);
            KPrint(L"Pressed: %u", 40, 330, 0xFFFFFF, 26, JETBRAINS_MONO, Keyboard.Pressed ? 1 : 0);

            KPrint(L"MOUSE", 40, 410, 0x80C0FF, 32, PRETENDARD);
            KPrint(L"X: %u", 40, 460, 0xFFFFFF, 26, JETBRAINS_MONO, Mouse.X);
            KPrint(L"Y: %u", 40, 500, 0xFFFFFF, 26, JETBRAINS_MONO, Mouse.Y);

            KPrint(L"Left: %u", 40, 540, 0xFFFFFF, 24, JETBRAINS_MONO, Mouse.LeftButton ? 1 : 0);
            KPrint(L"Right: %u", 250, 540, 0xFFFFFF, 24, JETBRAINS_MONO, Mouse.RightButton ? 1 : 0);
            KPrint(L"Middle: %u", 480, 540, 0xFFFFFF, 24, JETBRAINS_MONO, Mouse.MiddleButton ? 1 : 0);
            KPrint(L"Wheel: %d", 40, 580, 0xFFFFFF, 24, JETBRAINS_MONO, Mouse.Wheel);
            KPrint(L"Back: %u", 250, 580, 0xFFFFFF, 24, JETBRAINS_MONO, Mouse.BackButton ? 1 : 0);
            KPrint(L"Forward: %u", 480, 580, 0xFFFFFF, 24, JETBRAINS_MONO, Mouse.ForwardButton ? 1 : 0);

            VirtIOGPUTransferRect(INPUT_PANEL_X, INPUT_PANEL_Y, INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT);
            VirtIOGPUFlushRect(INPUT_PANEL_X, INPUT_PANEL_Y, INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT);

            LastKeyboard = Keyboard;
            LastMouse = Mouse;
            FirstFrame = FALSE;
        }

        UpdateMouseCursor();

        TaskSleepMls(INPUT_TEST_INTERVAL_MS);
    }
}

STATIC VOID DiskTestFlush(VOID)
{
    VirtIOGPUTransferRect(20, 650, 1200, 260);
    VirtIOGPUFlushRect(20, 650, 1200, 260);
}

typedef struct PACKED
{
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 HeaderCRC32;
    UINT32 Reserved;
    UINT64 CurrentLBA;
    UINT64 BackupLBA;
    UINT64 FirstUsableLBA;
    UINT64 LastUsableLBA;
    UINT8 DiskGUID[16];
    UINT64 PartitionEntryLBA;
    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntryArrayCRC32;
    UINT8 Reserved2[420];
} GPT_HEADER;

typedef struct PACKED
{
    UINT8 TypeGUID[16];
    UINT8 UniqueGUID[16];
    UINT64 FirstLBA;
    UINT64 LastLBA;
    UINT64 Attributes;
    CHAR16 Name[36];
} GPT_PARTITION_ENTRY;

#define GPT_SIGNATURE 0x5452415020494645ULL
#define EXT2_MAGIC    0xEF53
#define DISK_TEST_DMA_LIMIT 0x100000000ULL
#define DISK_TEST_TIMEOUT   10000000U

STATIC VOID *DiskTestBuffer;

STATIC BOOLEAN DiskTestReadSector(UINT64 Sector)
{
    VIRTIO_BLOCK_DEVICE *Device;
    VIRTIO_BLOCK_REQUEST *Request;

    Device = VirtIOBlockGetDevice();

    if (Device == NULL || DiskTestBuffer == NULL)
    {
        return FALSE;
    }

    Request = NULL;

    if (!VirtIOBlockReadAsync(Device, Sector, 1, DiskTestBuffer, &Request))
    {
        KPrint(L"Read submit failed: LBA %u", 40, 760, 0xFF6060, 22, JETBRAINS_MONO, (UINT32) Sector);
        return FALSE;
    }

    if (!VirtIOBlockWait(Device, Request, DISK_TEST_TIMEOUT))
    {
        KPrint(L"Read failed: LBA %u", 40, 790, 0xFF6060, 22, JETBRAINS_MONO, (UINT32) Sector);

        if (Request->State == VIRTIO_BLOCK_REQUEST_FAILED ||
            Request->State == VIRTIO_BLOCK_REQUEST_COMPLETED)
        {
            VirtIOBlockReleaseRequest(Device, Request);
        }

        return FALSE;
    }

    VirtIOBlockReleaseRequest(Device, Request);
    return TRUE;
}

STATIC VOID VirtIOBlockTestTask(VOID)
{
    VIRTIO_BLOCK_DEVICE *Device;
    GPT_HEADER *GPTHeader;
    GPT_PARTITION_ENTRY *PartitionEntry;
    UINT64 Capacity;
    UINT64 PartitionEntrySector;
    UINT64 PartitionStartLBA;
    UINT16 EXT2Magic;

    KPrint(L"VirtIO-BLK Test", 40, 680, 0x80C0FF, 28, PRETENDARD);

    Device = VirtIOBlockGetDevice();

    if (Device == NULL)
    {
        KPrint(L"Block device unavailable", 40, 720, 0xFF6060, 22, PRETENDARD);

        for (;;)
        {
            TaskSleepMls(1000);
        }
    }

    Capacity = VirtIOBlockGetCapacity(Device);

    KPrint(L"Capacity: %u sectors", 40, 720, 0xFFFFFF, 22, JETBRAINS_MONO, (UINT32) Capacity);

    DiskTestBuffer = KAllocPagesBelow(1, DISK_TEST_DMA_LIMIT);

    if (DiskTestBuffer == NULL)
    {
        KPrint(L"DMA buffer allocation failed", 40, 760, 0xFF6060, 22, PRETENDARD);

        for (;;)
        {
            TaskSleepMls(1000);
        }
    }

    KMemSet(DiskTestBuffer, 0, 4096);

    if (!DiskTestReadSector(1))
    {
        goto TestFinished;
    }

    GPTHeader = (GPT_HEADER *) DiskTestBuffer;

    if (GPTHeader->Signature != GPT_SIGNATURE ||
        GPTHeader->HeaderSize < 92 ||
        GPTHeader->HeaderSize > 512)
    {
        KPrint(L"Invalid GPT header", 40, 760, 0xFF6060, 22, PRETENDARD);
        goto TestFinished;
    }

    KPrint(L"GPT header: OK", 40, 760, 0x80FF80, 22, PRETENDARD);

    if (GPTHeader->NumberOfPartitionEntries < 2 ||
        GPTHeader->SizeOfPartitionEntry != sizeof(GPT_PARTITION_ENTRY))
    {
        KPrint(L"Unsupported GPT entry layout", 40, 790, 0xFF6060, 22, PRETENDARD);
        goto TestFinished;
    }

    PartitionEntrySector = GPTHeader->PartitionEntryLBA + sizeof(GPT_PARTITION_ENTRY) / 512;

    if (PartitionEntrySector >= Capacity || !DiskTestReadSector(PartitionEntrySector))
    {
        KPrint(L"GPT entries read failed", 40, 790, 0xFF6060, 22, PRETENDARD);
        goto TestFinished;
    }

    PartitionEntry = (GPT_PARTITION_ENTRY *) ((UINT8 *) DiskTestBuffer + sizeof(GPT_PARTITION_ENTRY));

    if (PartitionEntry->FirstLBA == 0 ||
        PartitionEntry->FirstLBA >= Capacity ||
        PartitionEntry->LastLBA < PartitionEntry->FirstLBA ||
        PartitionEntry->LastLBA >= Capacity)
    {
        KPrint(L"Invalid root partition", 40, 820, 0xFF6060, 22, PRETENDARD);
        goto TestFinished;
    }

    PartitionStartLBA = PartitionEntry->FirstLBA;

    KPrint(L"Root start LBA: %u", 40, 790, 0xFFFFFF, 22, JETBRAINS_MONO, (UINT32) PartitionStartLBA);

    if (PartitionStartLBA > Capacity - 3 ||
        !DiskTestReadSector(PartitionStartLBA + 2))
    {
        KPrint(L"EXT2 superblock read failed", 40, 820, 0xFF6060, 22, PRETENDARD);
        goto TestFinished;
    }

    EXT2Magic = *(UINT16 *) ((UINT8 *) DiskTestBuffer + 56);

    if (EXT2Magic != EXT2_MAGIC)
    {
        KPrint(L"EXT2 magic invalid: 0x%X", 40, 850, 0xFF6060, 22, JETBRAINS_MONO, (UINT32) EXT2Magic);
        goto TestFinished;
    }

    KPrint(L"EXT2 superblock: OK", 40, 820, 0x80FF80, 22, PRETENDARD);
    KPrint(L"Storage read test passed!", 40, 860, 0x80FF80, 28, PRETENDARD);
    DiskTestFlush();
TestFinished:
    for (;;)
    {
        TaskSleepMls(1000);
    }
}

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();
    CPUEnableSSE();

    KMemoryInit(BootInfo);
    KHeapInit();

    TrueTypeInit();
    PCIInit(BootInfo);

    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    FillScreen(0x101010);
    VirtIOGPUFlush();

    GDTInitCurrentCPU();

    IDTInit();
    IDTLoad();

    LAPICInit();
    HPETInit(BootInfo);
    TSCCalibrate();
    TSCDeadlineInit(0x40);

    SMPInit(BootInfo);

    LBPWRRInit();

    VirtIOBlockInit();
    VirtIOInputInit();
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    TASK *KeyboardTask;
    TASK *MouseTask;
    TASK *TestTask;
    TASK *DiskTask;

    InitKernel(BootInfo);

    FillScreen(0x101010);

    KPrint(L"LineOS VirtIO Input Test", 40, 80, 0xFFFFFF, 40, PRETENDARD);

    KPrint(L"Keyboard: %u", 40, 140, 0x80FF80, 26, JETBRAINS_MONO, VirtIOInputIsKeyboardAvailable() ? 1 : 0);
    KPrint(L"Tablet: %u", 40, 180, 0x80FF80, 26, JETBRAINS_MONO, VirtIOInputIsTabletAvailable() ? 1 : 0);

    VirtIOGPUFlush();

    KeyboardTask = TaskCreate(InputKeyboardTask);
    MouseTask = TaskCreate(InputMouseTask);
    TestTask = TaskCreate(InputTestTask);
    DiskTask = TaskCreate(VirtIOBlockTestTask);

    if (KeyboardTask == NULL || MouseTask == NULL || TestTask == NULL || DiskTask == NULL)
    {
        Panic(L"Task creation failed");
    }

    if (!LBPWRRAddTask(KeyboardTask))
    {
        Panic(L"Failed to add keyboard task");
    }

    if (!LBPWRRAddTask(MouseTask))
    {
        Panic(L"Failed to add mouse task");
    }

    if (!LBPWRRAddTask(TestTask))
    {
        Panic(L"Failed to add input test task");
    }

    if (!LBPWRRAddTask(DiskTask))
    {
        Panic(L"Failed to add disk test task");
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
        CPU->Online = TRUE;
        CompilerBarrier();
    }

    CompilerBarrier();

    for (;;)
    {
        LBPWRRJoin();
        STIHLT();
    }
}
