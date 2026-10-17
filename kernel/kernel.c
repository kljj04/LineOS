// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/TrueType/print.h>
#include <render/TrueType/TrueTypeEngine.h>
#include <render/GPU/VirtIOGPU.h>
#include <multicore/SMP.h>
#include <input/VirtIOInput.h>
#include <input/input.h>
#include <scheduler/LBPWRR.h>
#include <scheduler/task.h>
#include <arch/AMD64/CPU.h>
#include <debug/panic.h>
#include <LineOS/BootInfo.h>
#include <interrupt/APIC.h>
#include <interrupt/IDT.h>
#include <memory/PMM.h>
#include <timer/HPET.h>
#include <timer/TSC.h>
#include <PCI/PCI.h>

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

        KeyboardChanged = FirstFrame ||
            Keyboard.KeyCode != LastKeyboard.KeyCode ||
            Keyboard.Pressed != LastKeyboard.Pressed;

        MouseChanged = FirstFrame ||
            Mouse.X != LastMouse.X ||
            Mouse.Y != LastMouse.Y;

        ButtonsChanged = FirstFrame ||
            Mouse.LeftButton != LastMouse.LeftButton ||
            Mouse.RightButton != LastMouse.RightButton ||
            Mouse.MiddleButton != LastMouse.MiddleButton;

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

    if (!TSCCalibrate() || !TSCDeadlineInit(0x40))
    {
        HLT();
    }

    if (!SMPInit(BootInfo))
    {
        HLT();
    }

    if (!LBPWRRInit())
    {
        HLT();
    }

    VirtIOInputInit();
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    TASK *KeyboardTask;
    TASK *MouseTask;
    TASK *TestTask;

    InitKernel(BootInfo);

    FillScreen(0x101010);

    KPrint(L"LineOS VirtIO Input Test", 40, 80, 0xFFFFFF, 40, PRETENDARD);

    KPrint(L"Keyboard: %u", 40, 140, 0x80FF80, 26, JETBRAINS_MONO, VirtIOInputIsKeyboardAvailable() ? 1 : 0);
    KPrint(L"Tablet: %u", 40, 180, 0x80FF80, 26, JETBRAINS_MONO, VirtIOInputIsTabletAvailable() ? 1 : 0);

    VirtIOGPUFlush();

    KeyboardTask = TaskCreate(InputKeyboardTask);
    MouseTask = TaskCreate(InputMouseTask);
    TestTask = TaskCreate(InputTestTask);

    if (KeyboardTask == NULL || MouseTask == NULL || TestTask == NULL)
    {
        Panic(L"VirtIO input test task creation failed");
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