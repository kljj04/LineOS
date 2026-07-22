// kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>
#include <acpi/acpi.h>
#include <arch/x86_64/cpu.h>
#include <arch/x86_64/interrupt.h>
#include <memory/memory.h>
#include <render/framebuffer.h>
#include <render/print.h>
#include <time/lapic.h>
#include <time/timer.h>

#define PS2_BG                  0x05070D
#define PS2_PANEL               0x0D1422
#define PS2_BORDER              0x2C4F73
#define PS2_TEXT                0xF0F6FF
#define PS2_DIM                 0x8EA4B8
#define PS2_GREEN               0x6AF0A8
#define PS2_RED                 0xFF5C7A
#define PS2_YELLOW              0xFFD166
#define PS2_CURSOR              0xFFFFFF
#define PS2_CURSOR_SHADOW       0x000000
#define PS2_CURSOR_WIDTH        16
#define PS2_CURSOR_HEIGHT       24

#define PS2_DATA_PORT           0x60
#define PS2_STATUS_PORT         0x64
#define PS2_COMMAND_PORT        0x64
#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_MOUSE_DATA   0x20

#define PS2_COMMAND_READ_CONFIG 0x20
#define PS2_COMMAND_WRITE_CONFIG 0x60
#define PS2_COMMAND_ENABLE_AUX  0xA8
#define PS2_COMMAND_WRITE_AUX   0xD4

#define PS2_MOUSE_ENABLE        0xF4
#define PS2_MOUSE_SET_DEFAULTS  0xF6
#define PS2_ACK                 0xFA

#define PIC1_DATA               0x21
#define PIC2_COMMAND            0xA0
#define PIC2_DATA               0xA1
#define PIC_EOI                 0x20
#define IRQ12_VECTOR            0x74

STATIC volatile INT32 MouseX;
STATIC volatile INT32 MouseY;
STATIC volatile UINT8 MouseButtons;
STATIC volatile UINT64 MousePackets;
STATIC volatile BOOLEAN MouseUpdated;
STATIC UINT32 ScreenWidth;
STATIC UINT32 ScreenHeight;

STATIC CONST UINT16 MouseCursorBorder[PS2_CURSOR_HEIGHT] =
{
    0x8000,
    0xC000,
    0xA000,
    0x9000,
    0x8800,
    0x8400,
    0x8200,
    0x8100,
    0x8080,
    0x8040,
    0x8020,
    0x803E,
    0xA500,
    0xA500,
    0xC500,
    0x8400,
    0x0400,
    0x0200,
    0x0200,
    0x0100,
    0x0100,
    0x00C0,
    0x0000,
    0x0000
};

STATIC CONST UINT16 MouseCursorFill[PS2_CURSOR_HEIGHT] =
{
    0x0000,
    0x0000,
    0x4000,
    0x6000,
    0x7000,
    0x7800,
    0x7C00,
    0x7E00,
    0x7F00,
    0x7F80,
    0x7FC0,
    0x7C00,
    0x6000,
    0x4000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
};

STATIC UINT8 In8(UINT16 port)
{
    UINT8 value;

    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

STATIC VOID Out8(UINT16 port, UINT8 value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

STATIC BOOLEAN PS2WaitRead(VOID)
{
    for (UINT32 i = 0; i < 100000; i++)
    {
        if ((In8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0)
        {
            return TRUE;
        }

        __asm__ volatile("pause");
    }

    return FALSE;
}

STATIC BOOLEAN PS2WaitWrite(VOID)
{
    for (UINT32 i = 0; i < 100000; i++)
    {
        if ((In8(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0)
        {
            return TRUE;
        }

        __asm__ volatile("pause");
    }

    return FALSE;
}

STATIC UINT8 PS2Read(VOID)
{
    if (!PS2WaitRead())
    {
        return 0;
    }

    return In8(PS2_DATA_PORT);
}

STATIC VOID PS2WriteCommand(UINT8 command)
{
    if (PS2WaitWrite())
    {
        Out8(PS2_COMMAND_PORT, command);
    }
}

STATIC VOID PS2WriteData(UINT8 data)
{
    if (PS2WaitWrite())
    {
        Out8(PS2_DATA_PORT, data);
    }
}

STATIC BOOLEAN PS2WriteMouse(UINT8 data)
{
    PS2WriteCommand(PS2_COMMAND_WRITE_AUX);
    PS2WriteData(data);
    return PS2Read() == PS2_ACK;
}

STATIC VOID PICUnmaskIRQ12(VOID)
{
    UINT8 MasterMask = In8(PIC1_DATA);
    UINT8 SlaveMask = In8(PIC2_DATA);

    MasterMask &= ~(1 << 2);
    SlaveMask &= ~(1 << 4);
    Out8(PIC1_DATA, MasterMask);
    Out8(PIC2_DATA, SlaveMask);
}

STATIC VOID PICEOIIRQ12(VOID)
{
    Out8(PIC2_COMMAND, PIC_EOI);
    Out8(PIC_EOI, PIC_EOI);
}

STATIC INT32 PS2SignExtend(UINT8 value)
{
    return (value & 0x80) != 0 ? (INT32)value - 256 : (INT32)value;
}

STATIC VOID PS2HandlePacket(UINT8 Packet0, UINT8 Packet1, UINT8 Packet2)
{
    INT32 dx = PS2SignExtend(Packet1);
    INT32 dy = PS2SignExtend(Packet2);

    if ((Packet0 & 0x08) == 0)
    {
        return;
    }

    MouseX += dx;
    MouseY -= dy;

    if (MouseX < 0)
    {
        MouseX = 0;
    }

    if (MouseY < 0)
    {
        MouseY = 0;
    }

    if (MouseX > (INT32)ScreenWidth - PS2_CURSOR_WIDTH)
    {
        MouseX = (INT32)ScreenWidth - PS2_CURSOR_WIDTH;
    }

    if (MouseY > (INT32)ScreenHeight - PS2_CURSOR_HEIGHT)
    {
        MouseY = (INT32)ScreenHeight - PS2_CURSOR_HEIGHT;
    }

    MouseButtons = Packet0 & 0x07;
    MousePackets++;
    MouseUpdated = TRUE;
}

STATIC VOID PS2PollMouse(VOID)
{
    STATIC UINT8 Packet[3];
    STATIC UINT8 PacketIndex;

    while ((In8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0)
    {
        UINT8 status = In8(PS2_STATUS_PORT);
        UINT8 data = In8(PS2_DATA_PORT);

        if ((status & PS2_STATUS_MOUSE_DATA) == 0)
        {
            continue;
        }

        if (PacketIndex == 0 && (data & 0x08) == 0)
        {
            continue;
        }

        Packet[PacketIndex++] = data;

        if (PacketIndex == 3)
        {
            PS2HandlePacket(Packet[0], Packet[1], Packet[2]);
            PacketIndex = 0;
        }
    }
}

__attribute__((interrupt)) VOID PS2MouseInterrupt(INTERRUPT_FRAME* frame)
{
    (VOID)frame;

    PS2PollMouse();
    PICEOIIRQ12();
    LAPICEOI();
}

STATIC BOOLEAN PS2MouseInit(VOID)
{
    UINT8 config;
    BOOLEAN DefaultsOK;
    BOOLEAN EnableOK;

    while ((In8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0)
    {
        (VOID)In8(PS2_DATA_PORT);
    }

    PS2WriteCommand(PS2_COMMAND_ENABLE_AUX);
    PS2WriteCommand(PS2_COMMAND_READ_CONFIG);
    config = PS2Read();
    config |= 0x02;
    config &= ~0x20;
    PS2WriteCommand(PS2_COMMAND_WRITE_CONFIG);
    PS2WriteData(config);

    DefaultsOK = PS2WriteMouse(PS2_MOUSE_SET_DEFAULTS);
    EnableOK = PS2WriteMouse(PS2_MOUSE_ENABLE);

    IDTSetGate(IRQ12_VECTOR, PS2MouseInterrupt, IDT_GATE_INTERRUPT);
    PICUnmaskIRQ12();
    STI();

    return DefaultsOK && EnableOK;
}

STATIC VOID ClearCursor(INT32 x, INT32 y)
{
    FillRect((UINT32)x, (UINT32)y, PS2_CURSOR_WIDTH, PS2_CURSOR_HEIGHT, PS2_BG);
}

STATIC VOID DrawCursor(INT32 x, INT32 y)
{
    for (UINT32 PixelY = 0; PixelY < PS2_CURSOR_HEIGHT; PixelY++)
    {
        for (UINT32 PixelX = 0; PixelX < PS2_CURSOR_WIDTH; PixelX++)
        {
            UINT16 mask = 0x8000 >> PixelX;

            if ((MouseCursorBorder[PixelY] & mask) != 0)
            {
                DrawPixel((UINT32)x + PixelX, (UINT32)y + PixelY, PS2_CURSOR_SHADOW);
            }
            else if ((MouseCursorFill[PixelY] & mask) != 0)
            {
                DrawPixel((UINT32)x + PixelX, (UINT32)y + PixelY, PS2_CURSOR);
            }
        }
    }
}

STATIC VOID DrawScreen(BOOLEAN MouseOK)
{
    FillScreen(PS2_BG);
    FillRect(56, 48, ScreenWidth - 112, 156, PS2_PANEL);
    DrawRect(56, 48, ScreenWidth - 112, 156, PS2_BORDER);
    KPrint(L"LineOS PS/2 Mouse Test", 88, 104, PS2_TEXT);
    KPrint(L"IRQ12 + polling fallback / move the mouse", 88, 144, PS2_DIM);
    KPrint(L"Init", 88, 184, PS2_DIM);
    KPrint(MouseOK ? L"OK" : L"failed or no ACK", 180, 184, MouseOK ? PS2_GREEN : PS2_RED);
}

STATIC VOID DrawStatus(VOID)
{
    FillRect(56, 236, 660, 156, PS2_BG);
    KPrint(L"x", 88, 280, PS2_DIM);
    KPrint(L"%d", 180, 280, PS2_TEXT, MouseX);
    KPrint(L"y", 88, 320, PS2_DIM);
    KPrint(L"%d", 180, 320, PS2_TEXT, MouseY);
    KPrint(L"buttons", 88, 360, PS2_DIM);
    KPrint(L"0x%x", 180, 360, PS2_YELLOW, MouseButtons);
    KPrint(L"packets", 360, 280, PS2_DIM);
    KPrint(L"%llu", 520, 280, PS2_GREEN, MousePackets);
}

VOID __attribute__((ms_abi)) KMain(LINEOS_BOOT_INFO* BootInfo)
{
    BOOLEAN MouseOK;
    INT32 LastX;
    INT32 LastY;

    FrameBufferInit(BootInfo);
    CPUDetect();
    PMMInit(BootInfo);
    ACPIInit();
    TimerInit();

    ScreenWidth = BootInfo->GOP->ScreenWidth;
    ScreenHeight = BootInfo->GOP->ScreenHeight;
    MouseX = (INT32)ScreenWidth / 2;
    MouseY = (INT32)ScreenHeight / 2;
    LastX = MouseX;
    LastY = MouseY;

    DrawScreen(FALSE);
    MouseOK = PS2MouseInit();
    DrawScreen(MouseOK);
    DrawStatus();
    DrawCursor(MouseX, MouseY);

    while (1)
    {
        PS2PollMouse();

        if (MouseUpdated)
        {
            MouseUpdated = FALSE;
            ClearCursor(LastX, LastY);
            DrawStatus();
            DrawCursor(MouseX, MouseY);
            LastX = MouseX;
            LastY = MouseY;
        }

        DelayMs(1);
    }
}
