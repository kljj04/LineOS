// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "assets/cursor/cursor.h"

#include <render/truetype/truetype_engine.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <input/virtio_input.h>
#include <scheduler/prr.h>
#include <arch/x86_64/cpu.h>
#include <lineos/bootinfo.h>
#include <interrupt/apic.h>
#include <interrupt/idt.h>
#include <memory/memory.h>
#include <debug/panic.h>
#include <timer/hpet.h>
#include <timer/tsc.h>
#include <acpi/acpi.h>
#include <pci/pci.h>

#include <scheduler/prr_types.h>
#include <scheduler/prr.h>

STATIC UINT32  MouseMinX = 0;
STATIC UINT32  MouseMaxX = 32767;
STATIC UINT32  MouseMinY = 0;
STATIC UINT32  MouseMaxY = 32767;
STATIC UINT32  MouseX = 0;
STATIC UINT32  MouseY = 0;
STATIC BOOLEAN ShiftPressed = FALSE;

STATIC CHAR16 KeyCodeToChar(UINT16 code, BOOLEAN shift)
{
    switch (code)
    {
    case 2:
        return shift ? L'!' : L'1';
    case 3:
        return shift ? L'@' : L'2';
    case 4:
        return shift ? L'#' : L'3';
    case 5:
        return shift ? L'$' : L'4';
    case 6:
        return shift ? L'%' : L'5';
    case 7:
        return shift ? L'^' : L'6';
    case 8:
        return shift ? L'&' : L'7';
    case 9:
        return shift ? L'*' : L'8';
    case 10:
        return shift ? L'(' : L'9';
    case 11:
        return shift ? L')' : L'0';
    case 12:
        return shift ? L'_' : L'-';
    case 13:
        return shift ? L'+' : L'=';
    case 16:
        return shift ? L'Q' : L'q';
    case 17:
        return shift ? L'W' : L'w';
    case 18:
        return shift ? L'E' : L'e';
    case 19:
        return shift ? L'R' : L'r';
    case 20:
        return shift ? L'T' : L't';
    case 21:
        return shift ? L'Y' : L'y';
    case 22:
        return shift ? L'U' : L'u';
    case 23:
        return shift ? L'I' : L'i';
    case 24:
        return shift ? L'O' : L'o';
    case 25:
        return shift ? L'P' : L'p';
    case 26:
        return shift ? L'{' : L'[';
    case 27:
        return shift ? L'}' : L']';
    case 30:
        return shift ? L'A' : L'a';
    case 31:
        return shift ? L'S' : L's';
    case 32:
        return shift ? L'D' : L'd';
    case 33:
        return shift ? L'F' : L'f';
    case 34:
        return shift ? L'G' : L'g';
    case 35:
        return shift ? L'H' : L'h';
    case 36:
        return shift ? L'J' : L'j';
    case 37:
        return shift ? L'K' : L'k';
    case 38:
        return shift ? L'L' : L'l';
    case 39:
        return shift ? L':' : L';';
    case 40:
        return shift ? L'"' : L'\'';
    case 41:
        return shift ? L'~' : L'`';
    case 43:
        return shift ? L'|' : L'\\';
    case 44:
        return shift ? L'Z' : L'z';
    case 45:
        return shift ? L'X' : L'x';
    case 46:
        return shift ? L'C' : L'c';
    case 47:
        return shift ? L'V' : L'v';
    case 48:
        return shift ? L'B' : L'b';
    case 49:
        return shift ? L'N' : L'n';
    case 50:
        return shift ? L'M' : L'm';
    case 51:
        return shift ? L'<' : L',';
    case 52:
        return shift ? L'>' : L'.';
    case 53:
        return shift ? L'?' : L'/';
    case 57:
        return L' ';
    }

    return 0;
}

STATIC UINT32 ScalePointerCoordinate(UINT32 value, UINT32 min, UINT32 max, UINT32 outputMax)
{
    if (value <= min)
    {
        return 0;
    }

    if (value >= max)
    {
        return outputMax;
    }

    if (max <= min)
    {
        return 0;
    }

    return (UINT32) (((UINT64) (value - min) * outputMax) / (max - min));
}

STATIC VOID HostLoopTask(VOID)
{
    while (TRUE)
    {
        CLI();
        if (!VirtIOGPUFlush())
        {
            Panic(VirtIOGPUGetLastError());
        }
        STI();
        HLTONCE();
    }
}

STATIC VOID MouseTask(VOID)
{
    VIRTIO_GPU_INFO     *GPU;
    VIRTIO_POINTER_EVENT PointerEvent;

    GPU = VirtIOGPUGetInfo();

    while (TRUE)
    {
        while (VirtIOInputGetPointerEvent(&PointerEvent))
        {
            if (MouseMaxX > MouseMinX)
            {
                MouseX = ScalePointerCoordinate(PointerEvent.X, MouseMinX, MouseMaxX, GPU->FrameBufferWidth - 1);
            }

            if (MouseMaxY > MouseMinY)
            {
                MouseY = ScalePointerCoordinate(PointerEvent.Y, MouseMinY, MouseMaxY, GPU->FrameBufferHeight - 1);
            }

            CLI();
            if (!VirtIOGPUMoveCursor(MouseX, MouseY))
            {
                Panic(VirtIOGPUGetLastError());
            }
            STI();
        }

        HLTONCE();
    }
}

STATIC VOID KeyboardTask(VOID)
{
    VIRTIO_KEY_EVENT KeyEvent;
    CHAR16           Character;
    CHAR16           Text[2];

    Text[0] = 0;
    Text[1] = 0;

    while (TRUE)
    {
        while (VirtIOInputGetKeyEvent(&KeyEvent))
        {
            if (KeyEvent.Code == 42 || KeyEvent.Code == 54)
            {
                ShiftPressed = KeyEvent.Pressed;
                continue;
            }

            if (!KeyEvent.Pressed)
            {
                continue;
            }

            Character = KeyCodeToChar(KeyEvent.Code, ShiftPressed);
            if (Character == 0)
            {
                continue;
            }

            Text[0] = Character;
            CLI();
            KPrint(Text, MouseX, MouseY, 0xFFFFFFFF, 32, PRETENDARD);
            STI();
        }

        HLTONCE();
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();

    if (!KMemoryInit(BootInfo))
    {
        Panic(L"Memory Init FAILED");
    }

    if (!TrueTypeInit())
    {
        Panic(L"TrueType Init FAILED");
    }

    if (!PCIInit(BootInfo))
    {
        Panic(L"PCI Init FAILED");
    }

    if (!VirtIOGPUInit())
    {
        Panic(VirtIOGPUGetLastError());
    }

    GPU = VirtIOGPUGetInfo();

    if (!VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height))
    {
        Panic(VirtIOGPUGetLastError());
    }

    FillScreen(0x1E1E1E);
    if (!VirtIOGPUFlush())
    {
        Panic(VirtIOGPUGetLastError());
    }

    IDTInit();
    IDTLoad();

    if (!LAPICInit())
    {
        Panic(L"LAPIC Init FAILED");
    }

    if (!HPETInit(BootInfo))
    {
        Panic(L"HPET Init FAILED");
    }

    if (!TSCCalibrate())
    {
        Panic(L"TSC Calibrate FAILED");
    }

    FillScreen(0x1E1E1E);

    if (!VirtIOInputInit())
    {
        Panic(L"VirtIO Input Init FAILED");
    }

    KPrint(L"VirtIO Input Init OK", 100, 100, 0x55FF55FF, 40, PRETENDARD);
    KPrint(L"Keyboard: %s", 100, 160, 0xFFFFFFFF, 40, PRETENDARD, VirtIOInputIsKeyboardAvailable() ? L"OK" : L"NONE");
    KPrint(L"Tablet: %s", 100, 220, 0xFFFFFFFF, 40, PRETENDARD, VirtIOInputIsTabletAvailable() ? L"OK" : L"NONE");

    if (VirtIOInputGetTabletRange(&MouseMinX, &MouseMaxX, &MouseMinY, &MouseMaxY))
    {
        KPrint(L"ABS X: %u - %u", 100, 300, 0xFFFFFFFF, 40, PRETENDARD, MouseMinX, MouseMaxX);
        KPrint(L"ABS Y: %u - %u", 100, 400, 0xFFFFFFFF, 40, PRETENDARD, MouseMinY, MouseMaxY);
    }

    if (!VirtIOGPUCreateCursor(CURSOR_WIDTH, CURSOR_HEIGHT, 0, 0))
    {
        Panic(L"VirtIO GPU Cursor Create FAILED");
    }

    KMemCpy(GPU->CursorBuffer, CursorBitmap, sizeof(CursorBitmap));

    if (!VirtIOGPUUpdateCursor(MouseX, MouseY))
    {
        Panic(VirtIOGPUGetLastError());
    }

    if (!PRRInit())
    {
        Panic(L"PRR Init FAILED");
    }

    if (!PRRCreateTask(HostLoopTask) || !PRRCreateTask(MouseTask) || !PRRCreateTask(KeyboardTask))
    {
        Panic(L"PRR Create Task FAILED");
    }

    if (!TSCDeadlineInit(0x40))
    {
        Panic(L"TSC Deadline Init FAILED");
    }

    StartSchedule();
    HLT();
}
