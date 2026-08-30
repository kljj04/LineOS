// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "assets/cursor/cursor.h"

#include <render/truetype/truetype_engine.h>
#include <arch/x86_64/cpu.h>
#include <debug/panic.h>
#include <interrupt/apic.h>
#include <interrupt/idt.h>
#include <acpi/acpi.h>
#include <input/virtio_input.h>
#include <lineos/bootinfo.h>
#include <memory/memory.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <timer/hpet.h>
#include <timer/tsc.h>

STATIC CHAR16 KeyCodeToChar(UINT16 Code, BOOLEAN Shift)
{
    switch (Code)
    {
    case 2:
        return Shift ? L'!' : L'1';
    case 3:
        return Shift ? L'@' : L'2';
    case 4:
        return Shift ? L'#' : L'3';
    case 5:
        return Shift ? L'$' : L'4';
    case 6:
        return Shift ? L'%' : L'5';
    case 7:
        return Shift ? L'^' : L'6';
    case 8:
        return Shift ? L'&' : L'7';
    case 9:
        return Shift ? L'*' : L'8';
    case 10:
        return Shift ? L'(' : L'9';
    case 11:
        return Shift ? L')' : L'0';

    case 12:
        return Shift ? L'_' : L'-';
    case 13:
        return Shift ? L'+' : L'=';

    case 16:
        return Shift ? L'Q' : L'q';
    case 17:
        return Shift ? L'W' : L'w';
    case 18:
        return Shift ? L'E' : L'e';
    case 19:
        return Shift ? L'R' : L'r';
    case 20:
        return Shift ? L'T' : L't';
    case 21:
        return Shift ? L'Y' : L'y';
    case 22:
        return Shift ? L'U' : L'u';
    case 23:
        return Shift ? L'I' : L'i';
    case 24:
        return Shift ? L'O' : L'o';
    case 25:
        return Shift ? L'P' : L'p';

    case 26:
        return Shift ? L'{' : L'[';
    case 27:
        return Shift ? L'}' : L']';

    case 30:
        return Shift ? L'A' : L'a';
    case 31:
        return Shift ? L'S' : L's';
    case 32:
        return Shift ? L'D' : L'd';
    case 33:
        return Shift ? L'F' : L'f';
    case 34:
        return Shift ? L'G' : L'g';
    case 35:
        return Shift ? L'H' : L'h';
    case 36:
        return Shift ? L'J' : L'j';
    case 37:
        return Shift ? L'K' : L'k';
    case 38:
        return Shift ? L'L' : L'l';

    case 39:
        return Shift ? L':' : L';';
    case 40:
        return Shift ? L'"' : L'\'';
    case 41:
        return Shift ? L'~' : L'`';

    case 43:
        return Shift ? L'|' : L'\\';

    case 44:
        return Shift ? L'Z' : L'z';
    case 45:
        return Shift ? L'X' : L'x';
    case 46:
        return Shift ? L'C' : L'c';
    case 47:
        return Shift ? L'V' : L'v';
    case 48:
        return Shift ? L'B' : L'b';
    case 49:
        return Shift ? L'N' : L'n';
    case 50:
        return Shift ? L'M' : L'm';

    case 51:
        return Shift ? L'<' : L',';
    case 52:
        return Shift ? L'>' : L'.';
    case 53:
        return Shift ? L'?' : L'/';

    case 57:
        return L' ';
    }

    return 0;
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO     *GPU;
    VIRTIO_KEY_EVENT     KeyEvent;
    VIRTIO_POINTER_EVENT PointerEvent;
    UINT32               MinX;
    UINT32               MaxX;
    UINT32               MinY;
    UINT32               MaxY;
    UINT32               MouseX;
    UINT32               MouseY;
    BOOLEAN              ShiftPressed;
    CHAR16               Character;
    CHAR16               Text[2];

    CLI();

    KMemoryInit(BootInfo);
    TrueTypeInit();

    PCIInit(BootInfo);
    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    FillScreen(0x1E1E1E);
    VirtIOGPUFlush();

    IDTInit();
    IDTLoad();

    LAPICInit();

    HPETInit(BootInfo);
    TSCCalibrate();

    FillScreen(0x1E1E1E);

    if (!VirtIOInputInit())
    {
        Panic(L"VirtIO Input Init FAILED");
    }

    KPrint(L"VirtIO Input Init OK", 100, 100, 0x55FF55FF, 40, PRETENDARD);
    KPrint(L"Keyboard: %s", 100, 160, 0xFFFFFFFF, 40, PRETENDARD, VirtIOInputIsKeyboardAvailable() ? L"OK" : L"NONE");
    KPrint(L"Tablet: %s", 100, 220, 0xFFFFFFFF, 40, PRETENDARD, VirtIOInputIsTabletAvailable() ? L"OK" : L"NONE");

    MinX = 0;
    MaxX = 32767;
    MinY = 0;
    MaxY = 32767;

    if (VirtIOInputGetTabletRange(&MinX, &MaxX, &MinY, &MaxY))
    {
        KPrint(L"ABS X: %u - %u", 100, 300, 0xFFFFFFFF, 40, PRETENDARD, MinX, MaxX);
        KPrint(L"ABS Y: %u - %u", 100, 400, 0xFFFFFFFF, 40, PRETENDARD, MinY, MaxY);
    }

    VirtIOGPUFlush();

    MouseX = 0;
    MouseY = 0;
    ShiftPressed = FALSE;

    Text[0] = 0;
    Text[1] = 0;

    if (!VirtIOGPUCreateCursor(CURSOR_WIDTH, CURSOR_HEIGHT, 0, 0))
    {
        Panic(L"VirtIO GPU Cursor Create FAILED");
    }

    KMemCpy(GPU->CursorBuffer, CursorBitmap, sizeof(CursorBitmap));

    if (!VirtIOGPUUpdateCursor(MouseX, MouseY))
    {
        Panic(VirtIOGPUGetLastError());
    }

    while (TRUE)
    {
        VirtIOInputPoll();

        while (VirtIOInputGetPointerEvent(&PointerEvent))
        {
            if (MaxX > MinX)
            {
                MouseX = (UINT32) (((UINT64) (PointerEvent.X - MinX) * (GPU->FrameBufferWidth - 1)) / (MaxX - MinX));
            }

            if (MaxY > MinY)
            {
                MouseY = (UINT32) (((UINT64) (PointerEvent.Y - MinY) * (GPU->FrameBufferHeight - 1)) / (MaxY - MinY));
            }

            VirtIOGPUMoveCursor(MouseX, MouseY);
        }

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

            KPrint(Text, MouseX, MouseY, 0xFFFFFFFF, 32, PRETENDARD);
            VirtIOGPUFlush();
        }
    }
}
