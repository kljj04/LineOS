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
#include <PCI/PCI.h>
#include <Debug/Debug.h>
#include <Memory/VMM.h>
#include <Disk/VirtIOBlock.h>
#include <FS/EXT2/EXT2.h>

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
#define INPUT_PANEL_Y      190
#define INPUT_PANEL_WIDTH  1100
#define INPUT_PANEL_HEIGHT 430

STATIC VOID InputTestTask(VOID)
{
    KEYBOARD_STATUS Keyboard;
    KEYBOARD_STATUS LastKeyboard = {0};
    MOUSE_STATUS    Mouse;
    MOUSE_STATUS    LastMouse = {0};

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

            KPrint(L"KEYBOARD", 40, 240, 0x80C0FF, 44, PRETENDARD);
            KPrint(L"Key Code: 0x%X", 40, 290, 0xFFFFFF, 36, JETBRAINS_MONO, Keyboard.KeyCode);
            KPrint(L"Pressed: %u", 40, 330, 0xFFFFFF, 36, JETBRAINS_MONO, Keyboard.Pressed ? 1 : 0);

            KPrint(L"MOUSE", 40, 410, 0x80C0FF, 44, PRETENDARD);
            KPrint(L"X: %u", 40, 460, 0xFFFFFF, 36, JETBRAINS_MONO, Mouse.X);
            KPrint(L"Y: %u", 40, 500, 0xFFFFFF, 36, JETBRAINS_MONO, Mouse.Y);

            KPrint(L"Left: %u", 40, 540, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.LeftButton ? 1 : 0);
            KPrint(L"Right: %u", 300, 540, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.RightButton ? 1 : 0);
            KPrint(L"Middle: %u", 600, 540, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.MiddleButton ? 1 : 0);
            KPrint(L"Wheel: %d", 40, 580, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.Wheel);
            KPrint(L"Back: %u", 300, 580, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.BackButton ? 1 : 0);
            KPrint(L"Forward: %u", 600, 580, 0xFFFFFF, 34, JETBRAINS_MONO, Mouse.ForwardButton ? 1 : 0);

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
    VirtIOGPUTransferRect(20, 630, 1400, 420);
    VirtIOGPUFlushRect(20, 630, 1400, 420);
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
    UINT8  DiskGUID[16];
    UINT64 PartitionEntryLBA;
    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntryArrayCRC32;
    UINT8  Reserved2[420];
} GPT_HEADER;

typedef struct PACKED
{
    UINT8  TypeGUID[16];
    UINT8  UniqueGUID[16];
    UINT64 FirstLBA;
    UINT64 LastLBA;
    UINT64 Attributes;
    CHAR16 Name[36];
} GPT_PARTITION_ENTRY;

#define GPT_SIGNATURE       0x5452415020494645ULL
#define EXT2_MAGIC          0xEF53
#define DISK_TEST_DMA_LIMIT 0x100000000ULL
#define DISK_TEST_TIMEOUT   10000000U

STATIC VOID *DiskTestBuffer;

STATIC BOOLEAN DiskTestReadSector(UINT64 Sector)
{
    VIRTIO_BLOCK_DEVICE  *Device;
    VIRTIO_BLOCK_REQUEST *Request;

    Device = VirtIOBlockGetDevice();

    if (Device == NULL || DiskTestBuffer == NULL)
    {
        return FALSE;
    }

    Request = NULL;

    if (!VirtIOBlockReadAsync(Device, Sector, 1, DiskTestBuffer, &Request))
    {
        KPrint(L"Read submit failed: LBA %u", 40, 760, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) Sector);
        return FALSE;
    }

    if (!VirtIOBlockWait(Device, Request, DISK_TEST_TIMEOUT))
    {
        KPrint(L"Read failed: LBA %u", 40, 790, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) Sector);

        if (Request->State == VIRTIO_BLOCK_REQUEST_FAILED || Request->State == VIRTIO_BLOCK_REQUEST_COMPLETED)
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
    GPT_HEADER          *GPTHeader;
    GPT_PARTITION_ENTRY *PartitionEntry;
    UINT64               Capacity;
    UINT64               PartitionEntrySector;
    UINT64               PartitionStartLBA;
    UINT16               EXT2Magic;
    EXT2_VOLUME          Volume = {0};
    EXT2_INODE           Inode;
    UINT8               *WriteBuffer = NULL;
    UINT8               *ReadBuffer = NULL;
    CHAR8                TestPath[] = "/LineOSTest0000";
    CHAR8                FilePath[] = "/LineOSTest0000/Payload.bin";
    CHAR8                LinkPath[] = "/LineOSTest0000/Link";
    UINT32               DirectoryNumber;
    UINT32               FileNumber;
    UINT32               ScratchNumber;
    UINT32               SymlinkNumber;
    UINT32               FoundNumber;
    UINT32               ReadLength;
    UINT32               DataLength;
    UINT32               TestIndex;
    UINT64               SparseOffset;
    CONST CHAR16        *Stage = L"Storage read";
    BOOLEAN              TestPassed = FALSE;

    KPrint(L"VirtIO-BLK Test", 40, 680, 0x80C0FF, 40, PRETENDARD);

    Device = VirtIOBlockGetDevice();

    if (Device == NULL)
    {
        KPrint(L"Block device unavailable", 40, 720, 0xFF6060, 32, PRETENDARD);

        for (;;)
        {
            TaskSleepMls(1000);
        }
    }

    Capacity = VirtIOBlockGetCapacity(Device);

    KPrint(L"Capacity: %u sectors", 40, 720, 0xFFFFFF, 32, JETBRAINS_MONO, (UINT32) Capacity);

    DiskTestBuffer = KAllocPagesBelow(1, DISK_TEST_DMA_LIMIT);

    if (DiskTestBuffer == NULL)
    {
        KPrint(L"DMA buffer allocation failed", 40, 760, 0xFF6060, 32, PRETENDARD);

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

    if (GPTHeader->Signature != GPT_SIGNATURE || GPTHeader->HeaderSize < 92 || GPTHeader->HeaderSize > 512)
    {
        KPrint(L"Invalid GPT header", 40, 760, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    KPrint(L"GPT header: OK", 40, 760, 0x80FF80, 32, PRETENDARD);

    if (GPTHeader->NumberOfPartitionEntries < 2 || GPTHeader->SizeOfPartitionEntry != sizeof(GPT_PARTITION_ENTRY))
    {
        KPrint(L"Unsupported GPT entry layout", 40, 790, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    PartitionEntrySector = GPTHeader->PartitionEntryLBA + sizeof(GPT_PARTITION_ENTRY) / 512;

    if (PartitionEntrySector >= Capacity || !DiskTestReadSector(PartitionEntrySector))
    {
        KPrint(L"GPT entries read failed", 40, 790, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    PartitionEntry = (GPT_PARTITION_ENTRY *) ((UINT8 *) DiskTestBuffer + sizeof(GPT_PARTITION_ENTRY));

    if (PartitionEntry->FirstLBA == 0 || PartitionEntry->FirstLBA >= Capacity || PartitionEntry->LastLBA < PartitionEntry->FirstLBA || PartitionEntry->LastLBA >= Capacity)
    {
        KPrint(L"Invalid root partition", 40, 820, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    PartitionStartLBA = PartitionEntry->FirstLBA;

    KPrint(L"Root start LBA: %u", 40, 790, 0xFFFFFF, 32, JETBRAINS_MONO, (UINT32) PartitionStartLBA);

    if (PartitionStartLBA > Capacity - 3 || !DiskTestReadSector(PartitionStartLBA + 2))
    {
        KPrint(L"EXT2 superblock read failed", 40, 820, 0xFF6060, 32, PRETENDARD);
        goto TestFinished;
    }

    EXT2Magic = *(UINT16 *) ((UINT8 *) DiskTestBuffer + 56);

    if (EXT2Magic != EXT2_MAGIC)
    {
        KPrint(L"EXT2 magic invalid: 0x%X", 40, 850, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) EXT2Magic);
        goto TestFinished;
    }

    KPrint(L"EXT2 superblock: OK", 40, 820, 0x80FF80, 32, PRETENDARD);
    KPrint(L"Storage read test passed!", 40, 860, 0x80FF80, 40, PRETENDARD);
    DiskTestFlush();

    FillRect(20, 630, 1400, 420, 0x101010);
    KPrint(L"EXT2 filesystem test", 40, 680, 0x80C0FF, 40, PRETENDARD);
    DebugWriteLine("[EXT2 TEST] begin");
    DiskTestFlush();

    Stage = L"Mount GPT p2";

    if (!EXT2MountP2(&Volume, Device, TRUE))
    {
        goto TestFinished;
    }

    KPrint(L"GPT p2 mounted: %u-byte blocks", 40, 720, 0x80FF80, 32, JETBRAINS_MONO, Volume.BlockSize);
    DebugWriteLine("[EXT2 TEST] writable mount passed");
    DiskTestFlush();

    Stage = L"Root directory lookup";

    if (!EXT2Lookup(&Volume, (CONST CHAR8 *) "/", &FoundNumber, &Inode) || FoundNumber != EXT2_ROOT_INODE || (EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) != EXT2_MODE_DIRECTORY)
    {
        goto TestFinished;
    }

    Stage = L"Find unused test directory";

    for (TestIndex = 0; TestIndex < 10000; TestIndex++)
    {
        TestPath[11] = (CHAR8) ('0' + TestIndex / 1000);
        TestPath[12] = (CHAR8) ('0' + TestIndex / 100 % 10);
        TestPath[13] = (CHAR8) ('0' + TestIndex / 10 % 10);
        TestPath[14] = (CHAR8) ('0' + TestIndex % 10);

        if (!EXT2Lookup(&Volume, TestPath, &FoundNumber, &Inode))
        {
            if (Volume.Error != EXT2_ERROR_NOT_FOUND)
            {
                goto TestFinished;
            }

            break;
        }
    }

    if (TestIndex == 10000)
    {
        goto TestFinished;
    }

    KMemCpy(FilePath, TestPath, sizeof(TestPath) - 1);

    Stage = L"Create directory and file";

    if (!EXT2Create(&Volume, EXT2_ROOT_INODE, TestPath + 1, TRUE, 0755, 0, &DirectoryNumber) || !EXT2Create(&Volume, DirectoryNumber, (CONST CHAR8 *) "Payload.bin", FALSE, 0644, 0, &FileNumber))
    {
        goto TestFinished;
    }

    if (!EXT2Lookup(&Volume, FilePath, &FoundNumber, &Inode) || FoundNumber != FileNumber)
    {
        goto TestFinished;
    }

    KPrint(L"Root lookup, directory and file creation: OK", 40, 755, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWrite("[EXT2 TEST] created ");
    DebugWriteLine((CONST char *) FilePath);
    DiskTestFlush();

    Stage = L"Allocate test buffers";
    DataLength = Volume.BlockSize * 14 + 173;
    WriteBuffer = KAlloc(DataLength);
    ReadBuffer = KAlloc(DataLength);

    if (WriteBuffer == NULL || ReadBuffer == NULL)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        WriteBuffer[Index] = (UINT8) (Index * 17U + Index / 511);
    }

    Stage = L"Direct and single-indirect write/read";

    if (!EXT2WriteFile(&Volume, FileNumber, 0, WriteBuffer, DataLength, 0) || !EXT2ReadFile(&Volume, FileNumber, 0, ReadBuffer, DataLength, &ReadLength) || ReadLength != DataLength)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        if (ReadBuffer[Index] != WriteBuffer[Index])
        {
            goto TestFinished;
        }
    }

    KPrint(L"Direct + single-indirect: %u bytes verified", 40, 790, 0x80FF80, 32, JETBRAINS_MONO, DataLength);
    DebugWriteLine("[EXT2 TEST] direct and single-indirect data verified");
    DiskTestFlush();

    Stage = L"Double-indirect sparse write/read";
    SparseOffset = (12ULL + Volume.BlockSize / 4) * Volume.BlockSize;

    if (!EXT2WriteFile(&Volume, FileNumber, SparseOffset + 37, WriteBuffer, 173, 0) || !EXT2ReadFile(&Volume, FileNumber, SparseOffset - Volume.BlockSize, ReadBuffer, Volume.BlockSize + 210, &ReadLength) || ReadLength != Volume.BlockSize + 210)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < Volume.BlockSize + 37; Index++)
    {
        if (ReadBuffer[Index] != 0)
        {
            goto TestFinished;
        }
    }

    for (UINT32 Index = 0; Index < 173; Index++)
    {
        if (ReadBuffer[Volume.BlockSize + 37 + Index] != WriteBuffer[Index])
        {
            goto TestFinished;
        }
    }

    KPrint(L"Double-indirect data + zero-filled hole: OK", 40, 825, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] double-indirect sparse data verified");
    DiskTestFlush();

    Stage = L"EOF and overflow rejection";

    if (!EXT2ReadFile(&Volume, FileNumber, SparseOffset + 200, ReadBuffer, 173, &ReadLength) || ReadLength != 10)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < ReadLength; Index++)
    {
        if (ReadBuffer[Index] != WriteBuffer[163 + Index])
        {
            goto TestFinished;
        }
    }

    if (!EXT2ReadFile(&Volume, FileNumber, SparseOffset + 210, ReadBuffer, 1, &ReadLength) || ReadLength != 0 || EXT2WriteFile(&Volume, FileNumber, UINT64_MAX, WriteBuffer, 1, 0) || Volume.Error != EXT2_ERROR_LIMIT)
    {
        goto TestFinished;
    }

    Stage = L"Duplicate name rejection";

    if (EXT2Create(&Volume, DirectoryNumber, (CONST CHAR8 *) "Payload.bin", FALSE, 0644, 0, &FoundNumber) || Volume.Error != EXT2_ERROR_EXISTS)
    {
        goto TestFinished;
    }

    KPrint(L"EOF, overflow and duplicate rejection: OK", 40, 860, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] EOF and invalid operations verified");
    DiskTestFlush();

    Stage = L"Truncate, links, rename and deletion";

    if (!EXT2Create(&Volume, DirectoryNumber, (CONST CHAR8 *) "Scratch.bin", FALSE, 0600, 0, &ScratchNumber) || !EXT2WriteFile(&Volume, ScratchNumber, 0, WriteBuffer, DataLength, 0) || !EXT2Truncate(&Volume, ScratchNumber, Volume.BlockSize + 17, 0) || !EXT2Truncate(&Volume, ScratchNumber, DataLength, 0) || !EXT2ReadFile(&Volume, ScratchNumber, 0, ReadBuffer, DataLength, &ReadLength) || ReadLength != DataLength)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        if (ReadBuffer[Index] != (Index < Volume.BlockSize + 17 ? WriteBuffer[Index] : 0))
        {
            goto TestFinished;
        }
    }

    if (!EXT2Link(&Volume, DirectoryNumber, (CONST CHAR8 *) "Alias.bin", ScratchNumber, 0) || !EXT2Remove(&Volume, DirectoryNumber, (CONST CHAR8 *) "Scratch.bin", 0) || !EXT2Rename(&Volume, DirectoryNumber, (CONST CHAR8 *) "Alias.bin", DirectoryNumber, (CONST CHAR8 *) "Moved.bin", 0) || !EXT2Symlink(&Volume, DirectoryNumber, (CONST CHAR8 *) "Link", (CONST CHAR8 *) "Moved.bin", 0, &SymlinkNumber))
    {
        goto TestFinished;
    }

    KMemCpy(LinkPath, TestPath, sizeof(TestPath) - 1);

    if (!EXT2ReadLink(&Volume, SymlinkNumber, (CHAR8 *) ReadBuffer, DataLength, &ReadLength) || ReadLength != 9 || !EXT2Lookup(&Volume, LinkPath, &FoundNumber, &Inode) || FoundNumber != ScratchNumber)
    {
        goto TestFinished;
    }

    Stage = L"Owner and permission checks";

    if (!EXT2SetOwner(&Volume, ScratchNumber, 1000, 1000, 0) || !EXT2SetMode(&Volume, ScratchNumber, 0600, 0) || !EXT2SetCredentials(&Volume, 2000, 2000))
    {
        goto TestFinished;
    }

    if (EXT2WriteFile(&Volume, ScratchNumber, 0, WriteBuffer, 1, 0) || Volume.Error != EXT2_ERROR_PERMISSION)
    {
        goto TestFinished;
    }

    if (!EXT2SetCredentials(&Volume, 0, 0) || !EXT2Remove(&Volume, DirectoryNumber, (CONST CHAR8 *) "Link", 0) || !EXT2Remove(&Volume, DirectoryNumber, (CONST CHAR8 *) "Moved.bin", 0))
    {
        goto TestFinished;
    }

    if (EXT2Lookup(&Volume, LinkPath, &FoundNumber, &Inode) || Volume.Error != EXT2_ERROR_NOT_FOUND)
    {
        goto TestFinished;
    }

    KPrint(L"Truncate, links, rename, deletion, permissions: OK", 40, 895, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] truncate, links, rename, deletion and permissions passed");
    DiskTestFlush();

    Stage = L"Unmount and remount read-only";

    if (!EXT2Unmount(&Volume) || !EXT2MountP2(&Volume, Device, FALSE) || !EXT2Lookup(&Volume, FilePath, &FoundNumber, &Inode) || FoundNumber != FileNumber || !EXT2ReadFile(&Volume, FileNumber, 0, ReadBuffer, DataLength, &ReadLength) || ReadLength != DataLength)
    {
        goto TestFinished;
    }

    for (UINT32 Index = 0; Index < DataLength; Index++)
    {
        if (ReadBuffer[Index] != WriteBuffer[Index])
        {
            goto TestFinished;
        }
    }

    if (EXT2WriteFile(&Volume, FileNumber, 0, WriteBuffer, 1, 0) || Volume.Error != EXT2_ERROR_READ_ONLY)
    {
        goto TestFinished;
    }

    KPrint(L"Remount persistence + read-only rejection: OK", 40, 930, 0x80FF80, 32, JETBRAINS_MONO);
    DebugWriteLine("[EXT2 TEST] remount persistence and read-only checks passed");
    TestPassed = TRUE;

TestFinished:
    if (!TestPassed)
    {
        KPrint(L"FAIL: %s (EXT2 error %u)", 40, 970, 0xFF6060, 32, JETBRAINS_MONO, Stage, (UINT32) Volume.Error);
        DebugWrite("[EXT2 TEST] FAIL: ");
        DebugWriteWide(Stage);
        DebugWrite(" error=");
        DebugWriteDec(Volume.Error);
        DebugWriteLine("");
    }

    if (Volume.Mounted && !Volume.Poisoned)
    {
        if (!EXT2Unmount(&Volume))
        {
            TestPassed = FALSE;
            KPrint(L"FAIL: final unmount (error %u)", 40, 970, 0xFF6060, 32, JETBRAINS_MONO, (UINT32) Volume.Error);
            DebugWriteLine("[EXT2 TEST] FAIL: final unmount");
        }
    }

    if (WriteBuffer != NULL)
    {
        KFree(WriteBuffer);
    }

    if (ReadBuffer != NULL)
    {
        KFree(ReadBuffer);
    }

    if (DiskTestBuffer != NULL)
    {
        KMemFreePages(DiskTestBuffer, 1);
        DiskTestBuffer = NULL;
    }

    if (TestPassed)
    {
        KPrint(L"EXT2 TEST PASSED - retained /LineOSTest%c%c%c%c", 40, 970, 0x80FF80, 32, JETBRAINS_MONO, (UINT32) TestPath[11], (UINT32) TestPath[12], (UINT32) TestPath[13], (UINT32) TestPath[14]);
        DebugWriteLine("[EXT2 TEST] PASS");
    }

    DiskTestFlush();

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

    KPrint(L"LineOS VirtIO Input Test", 40, 80, 0xFFFFFF, 56, PRETENDARD);

    KPrint(L"Keyboard: %u", 40, 140, 0x80FF80, 36, JETBRAINS_MONO, VirtIOInputIsKeyboardAvailable() ? 1 : 0);
    KPrint(L"Tablet: %u", 40, 180, 0x80FF80, 36, JETBRAINS_MONO, VirtIOInputIsTabletAvailable() ? 1 : 0);

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
