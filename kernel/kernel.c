// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

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

#include <input/input.h>

#define DESKTOP_COLOR              0x181818FF

#define WINDOW_COLOR               0x252525FF
#define WINDOW_TITLE_COLOR         0x303030FF
#define WINDOW_BORDER_COLOR        0x505050FF
#define WINDOW_TEXT_COLOR          0xFFFFFFFF
#define WINDOW_SECONDARY_TEXT      0xB0B0B0FF

#define WINDOW_BUTTON_COLOR        0x383838FF
#define WINDOW_BUTTON_HOVER        0x484848FF
#define WINDOW_BUTTON_PRESS        0x585858FF

#define WINDOW_CLOSE_COLOR         0x404040FF
#define WINDOW_CLOSE_HOVER         0xC94F4FFF
#define WINDOW_CLOSE_PRESS         0xA83838FF

#define WINDOW_MAXIMIZE_COLOR      0x404040FF
#define WINDOW_MAXIMIZE_HOVER      0x555555FF

#define WINDOW_MIN_WIDTH           280
#define WINDOW_MIN_HEIGHT          180
#define WINDOW_TITLE_HEIGHT        42
#define WINDOW_RESIZE_SIZE         18
#define WINDOW_ANIMATION_STEP      28
#define WINDOW_SCREEN_MARGIN       24
#define WINDOW_MIN_VISIBLE         64

#define KEY_ESC                    1
#define KEY_F11                    87

typedef struct
{
    UINT32  X;
    UINT32  Y;
    UINT32  Width;
    UINT32  Height;

    BOOLEAN Hovered;
    BOOLEAN Pressed;
    BOOLEAN Armed;
} BUTTON;

typedef struct
{
    UINT32 X;
    UINT32 Y;
    UINT32 Width;
    UINT32 Height;

    UINT32 TargetX;
    UINT32 TargetY;
    UINT32 TargetWidth;
    UINT32 TargetHeight;

    UINT32 RestoreX;
    UINT32 RestoreY;
    UINT32 RestoreWidth;
    UINT32 RestoreHeight;

    BOOLEAN Visible;
    BOOLEAN Dragging;
    BOOLEAN Resizing;
    BOOLEAN Maximized;
    BOOLEAN MessageVisible;

    UINT32 DragOffsetX;
    UINT32 DragOffsetY;

    UINT32 ResizeStartX;
    UINT32 ResizeStartY;
    UINT32 ResizeStartWidth;
    UINT32 ResizeStartHeight;

    BUTTON MainButton;
    BUTTON CloseButton;
    BUTTON MaximizeButton;
} WINDOW;

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();

    KMemoryInit(BootInfo);

    TrueTypeInit();
    PCIInit(BootInfo);

    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    IDTInit();
    IDTLoad();

    LAPICInit();

    HPETInit(BootInfo);

    TSCCalibrate();
    TSCDeadlineInit(0x40);

    VirtIOInputInit();

    PRRInit();

    STI();
}

STATIC UINT32 MinUINT32(UINT32 a, UINT32 b)
{
    return a < b ? a : b;
}

STATIC UINT32 MaxUINT32(UINT32 a, UINT32 b)
{
    return a > b ? a : b;
}

STATIC UINT32 ClampUINT32(UINT32 value, UINT32 min, UINT32 max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}

STATIC BOOLEAN PointInside(UINT32 x, UINT32 y, UINT32 BoxX, UINT32 BoxY, UINT32 width, UINT32 height)
{
    return x >= BoxX && x < BoxX + width && y >= BoxY && y < BoxY + height;
}

STATIC UINT32 AnimateValue(UINT32 Current, UINT32 Target)
{
    UINT32 Difference;

    if (Current == Target)
    {
        return Current;
    }

    if (Current < Target)
    {
        Difference = Target - Current;

        if (Difference <= WINDOW_ANIMATION_STEP)
        {
            return Target;
        }

        return Current + WINDOW_ANIMATION_STEP;
    }

    Difference = Current - Target;

    if (Difference <= WINDOW_ANIMATION_STEP)
    {
        return Target;
    }

    return Current - WINDOW_ANIMATION_STEP;
}

STATIC BOOLEAN WindowIsAnimating(WINDOW *Window)
{
    return Window->X != Window->TargetX ||
           Window->Y != Window->TargetY ||
           Window->Width != Window->TargetWidth ||
           Window->Height != Window->TargetHeight;
}

STATIC BOOLEAN AnimateWindow(WINDOW *Window)
{
    UINT32 OldX;
    UINT32 OldY;
    UINT32 OldWidth;
    UINT32 OldHeight;

    OldX = Window->X;
    OldY = Window->Y;
    OldWidth = Window->Width;
    OldHeight = Window->Height;

    Window->X = AnimateValue(Window->X, Window->TargetX);
    Window->Y = AnimateValue(Window->Y, Window->TargetY);
    Window->Width = AnimateValue(Window->Width, Window->TargetWidth);
    Window->Height = AnimateValue(Window->Height, Window->TargetHeight);

    return OldX != Window->X || OldY != Window->Y || OldWidth != Window->Width || OldHeight != Window->Height;
}

STATIC BOOLEAN UpdateButton(BUTTON *Button, MOUSE_STATUS MouseStatus, BOOLEAN PreviousLeftButton)
{
    BOOLEAN Inside;
    BOOLEAN Clicked;

    Inside = PointInside(MouseStatus.X, MouseStatus.Y, Button->X, Button->Y, Button->Width, Button->Height);
    Clicked = FALSE;

    Button->Hovered = Inside;

    if (!PreviousLeftButton && MouseStatus.LeftButton && Inside)
    {
        Button->Armed = TRUE;
    }

    Button->Pressed = Button->Armed && MouseStatus.LeftButton;

    if (PreviousLeftButton && !MouseStatus.LeftButton)
    {
        if (Button->Armed && Inside)
        {
            Clicked = TRUE;
        }

        Button->Armed = FALSE;
        Button->Pressed = FALSE;
    }

    return Clicked;
}

STATIC VOID DrawButton(BUTTON *Button, CONST CHAR16 *Text)
{
    UINT32 color;

    if (Button->Pressed)
    {
        color = WINDOW_BUTTON_PRESS;
    }
    else if (Button->Hovered)
    {
        color = WINDOW_BUTTON_HOVER;
    }
    else
    {
        color = WINDOW_BUTTON_COLOR;
    }

    FillRect(Button->X, Button->Y, Button->Width, Button->Height, color);
    DrawRect(Button->X, Button->Y, Button->Width, Button->Height, WINDOW_BORDER_COLOR);

    KPrint(Text, Button->X + 14, Button->Y + 31, WINDOW_TEXT_COLOR, 22, JETBRAINS_MONO);
}

STATIC VOID DrawCloseButton(BUTTON *Button)
{
    UINT32 color;

    if (Button->Pressed)
    {
        color = WINDOW_CLOSE_PRESS;
    }
    else if (Button->Hovered)
    {
        color = WINDOW_CLOSE_HOVER;
    }
    else
    {
        color = WINDOW_CLOSE_COLOR;
    }

    FillRect(Button->X, Button->Y, Button->Width, Button->Height, color);
    KPrint(L"X", Button->X + 8, Button->Y + 20, WINDOW_TEXT_COLOR, 18, JETBRAINS_MONO);
}

STATIC VOID DrawMaximizeButton(BUTTON *Button, BOOLEAN Maximized)
{
    UINT32 color;

    if (Button->Pressed)
    {
        color = WINDOW_BUTTON_PRESS;
    }
    else if (Button->Hovered)
    {
        color = WINDOW_MAXIMIZE_HOVER;
    }
    else
    {
        color = WINDOW_MAXIMIZE_COLOR;
    }

    FillRect(Button->X, Button->Y, Button->Width, Button->Height, color);

    if (Maximized)
    {
        DrawRect(Button->X + 7, Button->Y + 7, 12, 10, WINDOW_TEXT_COLOR);
        DrawRect(Button->X + 10, Button->Y + 10, 12, 10, WINDOW_TEXT_COLOR);
    }
    else
    {
        DrawRect(Button->X + 7, Button->Y + 6, 14, 13, WINDOW_TEXT_COLOR);
    }
}

STATIC VOID UpdateWindowControls(WINDOW *Window)
{
    Window->MainButton.X = Window->X + 24;
    Window->MainButton.Y = Window->Y + WINDOW_TITLE_HEIGHT + 42;
    Window->MainButton.Width = 180;
    Window->MainButton.Height = 48;

    Window->CloseButton.X = Window->X + Window->Width - 38;
    Window->CloseButton.Y = Window->Y + 8;
    Window->CloseButton.Width = 28;
    Window->CloseButton.Height = 26;

    Window->MaximizeButton.X = Window->X + Window->Width - 72;
    Window->MaximizeButton.Y = Window->Y + 8;
    Window->MaximizeButton.Width = 28;
    Window->MaximizeButton.Height = 26;
}

STATIC VOID DrawWindow(WINDOW *Window, BOOLEAN FastMode)
{
    UINT32 ResizeX;
    UINT32 ResizeY;

    if (!Window->Visible)
    {
        return;
    }

    FillRect(Window->X, Window->Y, Window->Width, Window->Height, WINDOW_COLOR);
    DrawRect(Window->X, Window->Y, Window->Width, Window->Height, WINDOW_BORDER_COLOR);

    FillRect(Window->X + 1, Window->Y + 1, Window->Width - 2, WINDOW_TITLE_HEIGHT, WINDOW_TITLE_COLOR);

    ResizeX = Window->X + Window->Width - WINDOW_RESIZE_SIZE;
    ResizeY = Window->Y + Window->Height - WINDOW_RESIZE_SIZE;

    FillRect(ResizeX, ResizeY, WINDOW_RESIZE_SIZE, WINDOW_RESIZE_SIZE, WINDOW_BORDER_COLOR);

    if (FastMode)
    {
        return;
    }

    UpdateWindowControls(Window);

    KPrint(L"LineOS Window", Window->X + 16, Window->Y + 29, WINDOW_TEXT_COLOR, 22, JETBRAINS_MONO);

    DrawMaximizeButton(&Window->MaximizeButton, Window->Maximized);
    DrawCloseButton(&Window->CloseButton);

    DrawButton(&Window->MainButton, L"Click Me");

    if (Window->MessageVisible)
    {
        KPrint(L"ENG", Window->X + 24, Window->Y + WINDOW_TITLE_HEIGHT + 145, WINDOW_SECONDARY_TEXT, 70, JETBRAINS_MONO);
    }
    else
    {
        KPrint(L"KOR", Window->X + 24, Window->Y + WINDOW_TITLE_HEIGHT + 145, WINDOW_SECONDARY_TEXT, 70, JETBRAINS_MONO);
    }

    KPrint(L"Drag title bar | F11 maximize | ESC close", Window->X + 24, Window->Y + Window->Height - 34, WINDOW_SECONDARY_TEXT, 18, JETBRAINS_MONO);
}

STATIC VOID EraseWindow(UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
    FillRect(x, y, width, height, DESKTOP_COLOR);
}

STATIC VOID FlushWindowRegion(UINT32 OldX, UINT32 OldY, UINT32 OldWidth, UINT32 OldHeight, WINDOW *Window)
{
    UINT32 MinX;
    UINT32 MinY;
    UINT32 MaxX;
    UINT32 MaxY;

    MinX = MinUINT32(OldX, Window->X);
    MinY = MinUINT32(OldY, Window->Y);

    MaxX = MaxUINT32(OldX + OldWidth, Window->X + Window->Width);
    MaxY = MaxUINT32(OldY + OldHeight, Window->Y + Window->Height);

    VirtIOGPUTransferRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
    VirtIOGPUFlushRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
}

STATIC VOID ToggleWindowMaximize(WINDOW *Window, UINT32 ScreenWidth, UINT32 ScreenHeight)
{
    if (!Window->Maximized)
    {
        Window->RestoreX = Window->TargetX;
        Window->RestoreY = Window->TargetY;
        Window->RestoreWidth = Window->TargetWidth;
        Window->RestoreHeight = Window->TargetHeight;

        Window->TargetX = WINDOW_SCREEN_MARGIN;
        Window->TargetY = WINDOW_SCREEN_MARGIN;
        Window->TargetWidth = ScreenWidth - WINDOW_SCREEN_MARGIN * 2;
        Window->TargetHeight = ScreenHeight - WINDOW_SCREEN_MARGIN * 2;

        Window->Maximized = TRUE;
    }
    else
    {
        Window->TargetX = Window->RestoreX;
        Window->TargetY = Window->RestoreY;
        Window->TargetWidth = Window->RestoreWidth;
        Window->TargetHeight = Window->RestoreHeight;

        Window->Maximized = FALSE;
    }

    Window->Dragging = FALSE;
    Window->Resizing = FALSE;
}

STATIC BOOLEAN HandleWindowInput(WINDOW *Window, MOUSE_STATUS MouseStatus, BOOLEAN PreviousLeftButton, UINT32 ScreenWidth, UINT32 ScreenHeight)
{
    BOOLEAN Changed;
    BOOLEAN ResizeHovered;
    BOOLEAN TitleHovered;
    BOOLEAN CloseClicked;
    BOOLEAN MaximizeClicked;
    BOOLEAN MainClicked;

    BOOLEAN OldMainHovered;
    BOOLEAN OldMainPressed;
    BOOLEAN OldCloseHovered;
    BOOLEAN OldClosePressed;
    BOOLEAN OldMaximizeHovered;
    BOOLEAN OldMaximizePressed;

    UINT32 MaxX;
    UINT32 MaxY;
    INT64 NewWidth;
    INT64 NewHeight;

    Changed = FALSE;

    UpdateWindowControls(Window);

    OldMainHovered = Window->MainButton.Hovered;
    OldMainPressed = Window->MainButton.Pressed;

    OldCloseHovered = Window->CloseButton.Hovered;
    OldClosePressed = Window->CloseButton.Pressed;

    OldMaximizeHovered = Window->MaximizeButton.Hovered;
    OldMaximizePressed = Window->MaximizeButton.Pressed;

    MainClicked = UpdateButton(&Window->MainButton, MouseStatus, PreviousLeftButton);
    CloseClicked = UpdateButton(&Window->CloseButton, MouseStatus, PreviousLeftButton);
    MaximizeClicked = UpdateButton(&Window->MaximizeButton, MouseStatus, PreviousLeftButton);

    if (OldMainHovered != Window->MainButton.Hovered || OldMainPressed != Window->MainButton.Pressed)
    {
        Changed = TRUE;
    }

    if (OldCloseHovered != Window->CloseButton.Hovered || OldClosePressed != Window->CloseButton.Pressed)
    {
        Changed = TRUE;
    }

    if (OldMaximizeHovered != Window->MaximizeButton.Hovered || OldMaximizePressed != Window->MaximizeButton.Pressed)
    {
        Changed = TRUE;
    }

    if (CloseClicked)
    {
        Window->Visible = FALSE;
        return TRUE;
    }

    if (MaximizeClicked)
    {
        ToggleWindowMaximize(Window, ScreenWidth, ScreenHeight);
        return TRUE;
    }

    if (MainClicked)
    {
        Window->MessageVisible = !Window->MessageVisible;
        Changed = TRUE;
    }

    ResizeHovered = PointInside(MouseStatus.X, MouseStatus.Y, Window->X + Window->Width - WINDOW_RESIZE_SIZE, Window->Y + Window->Height - WINDOW_RESIZE_SIZE, WINDOW_RESIZE_SIZE, WINDOW_RESIZE_SIZE);
    TitleHovered = PointInside(MouseStatus.X, MouseStatus.Y, Window->X, Window->Y, Window->Width, WINDOW_TITLE_HEIGHT);

    if (!PreviousLeftButton && MouseStatus.LeftButton && !Window->Maximized)
    {
        if (ResizeHovered)
        {
            Window->Resizing = TRUE;

            Window->ResizeStartX = MouseStatus.X;
            Window->ResizeStartY = MouseStatus.Y;

            Window->ResizeStartWidth = Window->Width;
            Window->ResizeStartHeight = Window->Height;

            Changed = TRUE;
        }
        else if (TitleHovered && !Window->CloseButton.Hovered && !Window->MaximizeButton.Hovered)
        {
            Window->Dragging = TRUE;

            Window->DragOffsetX = MouseStatus.X - Window->X;
            Window->DragOffsetY = MouseStatus.Y - Window->Y;

            Changed = TRUE;
        }
    }

    if (PreviousLeftButton && !MouseStatus.LeftButton)
    {
        if (Window->Dragging || Window->Resizing)
        {
            Changed = TRUE;
        }

        Window->Dragging = FALSE;
        Window->Resizing = FALSE;
    }

    if (Window->Dragging)
    {
        if (MouseStatus.X >= Window->DragOffsetX)
        {
            Window->X = MouseStatus.X - Window->DragOffsetX;
        }
        else
        {
            Window->X = 0;
        }

        if (MouseStatus.Y >= Window->DragOffsetY)
        {
            Window->Y = MouseStatus.Y - Window->DragOffsetY;
        }
        else
        {
            Window->Y = 0;
        }

        if (ScreenWidth > WINDOW_MIN_VISIBLE)
        {
            MaxX = ScreenWidth - WINDOW_MIN_VISIBLE;
            Window->X = ClampUINT32(Window->X, 0, MaxX);
        }

        if (ScreenHeight > WINDOW_TITLE_HEIGHT)
        {
            MaxY = ScreenHeight - WINDOW_TITLE_HEIGHT;
            Window->Y = ClampUINT32(Window->Y, 0, MaxY);
        }

        Window->TargetX = Window->X;
        Window->TargetY = Window->Y;

        Changed = TRUE;
    }

    if (Window->Resizing)
    {
        NewWidth = (INT64) Window->ResizeStartWidth + (INT64) MouseStatus.X - (INT64) Window->ResizeStartX;
        NewHeight = (INT64) Window->ResizeStartHeight + (INT64) MouseStatus.Y - (INT64) Window->ResizeStartY;

        if (NewWidth < WINDOW_MIN_WIDTH)
        {
            NewWidth = WINDOW_MIN_WIDTH;
        }

        if (NewHeight < WINDOW_MIN_HEIGHT)
        {
            NewHeight = WINDOW_MIN_HEIGHT;
        }

        if ((UINT64) NewWidth > ScreenWidth - Window->X)
        {
            NewWidth = ScreenWidth - Window->X;
        }

        if ((UINT64) NewHeight > ScreenHeight - Window->Y)
        {
            NewHeight = ScreenHeight - Window->Y;
        }

        Window->Width = (UINT32) NewWidth;
        Window->Height = (UINT32) NewHeight;

        Window->TargetWidth = Window->Width;
        Window->TargetHeight = Window->Height;

        Changed = TRUE;
    }

    return Changed;
}

VOID WindowTestFunction(VOID)
{
    VIRTIO_GPU_INFO *GPU;

    WINDOW Window;

    MOUSE_STATUS MouseStatus;
    MOUSE_STATUS PreviousMouseStatus;

    KEYBOARD_STATUS KeyboardStatus;
    KEYBOARD_STATUS PreviousKeyboardStatus;

    BOOLEAN PreviousLeftButton;

    BOOLEAN Changed;
    BOOLEAN Animated;
    BOOLEAN Animating;
    BOOLEAN CursorMoved;
    BOOLEAN FastMode;

    UINT32 OldX;
    UINT32 OldY;
    UINT32 OldWidth;
    UINT32 OldHeight;

    GPU = VirtIOGPUGetInfo();

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return;
    }

    FillScreen(DESKTOP_COLOR);

    VirtIOGPUTransferRect(0, 0, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
    VirtIOGPUFlushRect(0, 0, GPU->FrameBufferWidth, GPU->FrameBufferHeight);

    Window.TargetWidth = 520;
    Window.TargetHeight = 320;

    Window.TargetX = (GPU->FrameBufferWidth - Window.TargetWidth) / 2;
    Window.TargetY = (GPU->FrameBufferHeight - Window.TargetHeight) / 2;

    Window.Width = 100;
    Window.Height = 70;

    Window.X = Window.TargetX + (Window.TargetWidth - Window.Width) / 2;
    Window.Y = Window.TargetY + (Window.TargetHeight - Window.Height) / 2;

    Window.RestoreX = Window.TargetX;
    Window.RestoreY = Window.TargetY;
    Window.RestoreWidth = Window.TargetWidth;
    Window.RestoreHeight = Window.TargetHeight;

    Window.Visible = TRUE;
    Window.Dragging = FALSE;
    Window.Resizing = FALSE;
    Window.Maximized = FALSE;
    Window.MessageVisible = FALSE;

    Window.DragOffsetX = 0;
    Window.DragOffsetY = 0;

    Window.ResizeStartX = 0;
    Window.ResizeStartY = 0;
    Window.ResizeStartWidth = 0;
    Window.ResizeStartHeight = 0;

    Window.MainButton.X = 0;
    Window.MainButton.Y = 0;
    Window.MainButton.Width = 0;
    Window.MainButton.Height = 0;
    Window.MainButton.Hovered = FALSE;
    Window.MainButton.Pressed = FALSE;
    Window.MainButton.Armed = FALSE;

    Window.CloseButton.X = 0;
    Window.CloseButton.Y = 0;
    Window.CloseButton.Width = 0;
    Window.CloseButton.Height = 0;
    Window.CloseButton.Hovered = FALSE;
    Window.CloseButton.Pressed = FALSE;
    Window.CloseButton.Armed = FALSE;

    Window.MaximizeButton.X = 0;
    Window.MaximizeButton.Y = 0;
    Window.MaximizeButton.Width = 0;
    Window.MaximizeButton.Height = 0;
    Window.MaximizeButton.Hovered = FALSE;
    Window.MaximizeButton.Pressed = FALSE;
    Window.MaximizeButton.Armed = FALSE;

    MouseStatus = GetMouseStatus();
    PreviousMouseStatus = MouseStatus;
    PreviousLeftButton = MouseStatus.LeftButton;

    KeyboardStatus = GetKeyboardStatus();
    PreviousKeyboardStatus = KeyboardStatus;

    ShowMouseCursor();

    while (TRUE)
    {
        MouseStatus = GetMouseStatus();
        KeyboardStatus = GetKeyboardStatus();

        CursorMoved = MouseStatus.X != PreviousMouseStatus.X || MouseStatus.Y != PreviousMouseStatus.Y;

        OldX = Window.X;
        OldY = Window.Y;
        OldWidth = Window.Width;
        OldHeight = Window.Height;

        Changed = FALSE;
        Animated = FALSE;

        Animating = Window.Visible && WindowIsAnimating(&Window);

        if (Window.Visible && !Animating)
        {
            Changed = HandleWindowInput(&Window, MouseStatus, PreviousLeftButton, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
        }

        if (KeyboardStatus.Pressed && (!PreviousKeyboardStatus.Pressed || KeyboardStatus.KeyCode != PreviousKeyboardStatus.KeyCode))
        {
            if (KeyboardStatus.KeyCode == KEY_ESC && Window.Visible)
            {
                Window.Visible = FALSE;
                Changed = TRUE;
            }
            else if (KeyboardStatus.KeyCode == KEY_F11 && Window.Visible && !Animating)
            {
                ToggleWindowMaximize(&Window, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
                Changed = TRUE;
            }
        }

        if (Window.Visible)
        {
            Animated = AnimateWindow(&Window);
        }

        Animating = Window.Visible && WindowIsAnimating(&Window);

        FastMode = Window.Dragging || Window.Resizing || Animating;

        if (Changed || Animated)
        {
            HideMouseCursor();

            EraseWindow(OldX, OldY, OldWidth, OldHeight);

            if (Window.Visible)
            {
                DrawWindow(&Window, FastMode);
                FlushWindowRegion(OldX, OldY, OldWidth, OldHeight, &Window);
            }
            else
            {
                VirtIOGPUTransferRect(OldX, OldY, OldWidth, OldHeight);
                VirtIOGPUFlushRect(OldX, OldY, OldWidth, OldHeight);
            }

            ShowMouseCursor();
        }
        else if (CursorMoved)
        {
            UpdateMouseCursor();
        }

        PreviousMouseStatus = MouseStatus;
        PreviousLeftButton = MouseStatus.LeftButton;

        PreviousKeyboardStatus = KeyboardStatus;

        HLTONCE();
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);

    PRRCreateTask(InputMouseHandler);
    PRRCreateTask(InputKeyboardHandler);
    PRRCreateTask(WindowTestFunction);

    StartSchedule();

    HLT();
}