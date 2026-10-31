// UserPrograms/System/lwm/Source/LwmWindow.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Lwm.h>
#include <sys/mman.h>
#include <string.h>
#include <libintl.h>
#include <errno.h>

STATIC BOOLEAN LwmContains(LWM_RECT Rect, CONST POINTER_INFO *Pointer)
{
    return (INT64) Pointer->X >= Rect.X && (INT64) Pointer->Y >= Rect.Y && (INT64) Pointer->X < (INT64) Rect.X + Rect.Width && (INT64) Pointer->Y < (INT64) Rect.Y + Rect.Height;
}

BOOLEAN LwmCreateWindow(LWM *Manager)
{
    if (Manager->WindowCount == LWM_WINDOW_LIMIT)
    {
        errno = ENOSPC;

        return FALSE;
    }

    LWM_WINDOW Window = {0};

    Window.Stride = Manager->Width;
    Window.CapacityHeight = Manager->Height - LWM_DOCK_HEIGHT;
    Window.Bytes = (UINT64) Window.Stride * Window.CapacityHeight * sizeof(UINT32);
    Window.Pixels = mmap(NULL, Window.Bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (Window.Pixels == MAP_FAILED)
    {
        return FALSE;
    }

    Window.Rect.Width = Window.Stride < 720 ? Window.Stride : 720;
    Window.Rect.Height = Window.CapacityHeight < 400 ? Window.CapacityHeight : 400;
    Window.Rect.X = (Manager->Width - Window.Rect.Width) / 2;
    Window.Rect.Y = (Manager->Height - LWM_DOCK_HEIGHT - Window.Rect.Height) / 2;
    Window.Dirty = TRUE;

    if (Manager->WindowCount != 0)
    {
        Manager->Windows[Manager->WindowCount - 1].Dirty = TRUE;
    }

    Manager->Windows[Manager->WindowCount++] = Window;
    Manager->Dirty = TRUE;

    return TRUE;
}

VOID LwmPaintWindow(LWM *Manager, LWM_WINDOW *Window, BOOLEAN Focused)
{
    UINT32 Width = Window->Rect.Width;
    UINT32 Height = Window->Rect.Height;

    Manager->Surface = (TRUE_TYPE_SURFACE){Window->Pixels, Window->Stride, Window->CapacityHeight};

    LwmFill(Manager, (LWM_RECT){0, 0, Width, Height}, 0x00414D61U);
    LwmFill(Manager, (LWM_RECT){1, LWM_TITLE_HEIGHT, Width - 2, Height - LWM_TITLE_HEIGHT - 1}, 0x00252B38U);
    LwmFill(Manager, (LWM_RECT){1, 1, Width - 2, LWM_TITLE_HEIGHT - 1}, Focused ? 0x00354860U : 0x002F3540U);
    LwmText(Manager, Window->Endpoint != 0 ? Window->Title : gettext("lwm demo"), 16, 31, 25, 0x00FFFFFFU, Width - 120);
    LwmFill(Manager, (LWM_RECT){(INT32) Width - 110, 8, 28, 28}, 0x00485B72U);
    LwmText(Manager, "-", Width - 103, 29, 22, 0x00FFFFFFU, Width - 82);
    LwmFill(Manager, (LWM_RECT){(INT32) Width - 74, 8, 28, 28}, 0x00485B72U);
    LwmText(Manager, Window->Maximized ? "=" : "+", Width - 67, 29, 22, 0x00FFFFFFU, Width - 46);
    LwmFill(Manager, (LWM_RECT){(INT32) Width - 38, 8, 28, 28}, 0x00C95F65U);
    LwmText(Manager, "X", Width - 31, 29, 22, 0x00FFFFFFU, Width - 10);
    if (Window->Endpoint != 0)
    {
        UINT32 ContentWidth = Width - 2;
        UINT32 ContentHeight = Height - LWM_TITLE_HEIGHT - 1;

        for (UINT32 Row = 0; Row < ContentHeight; Row++)
        {
            UINT32 SourceY = (UINT64) Row * Window->ClientHeight / ContentHeight;

            if (ContentWidth == Window->ClientWidth && ContentHeight == Window->ClientHeight)
            {
                memcpy(Window->Pixels + (UINTN) (Row + LWM_TITLE_HEIGHT) * Window->Stride + 1, Window->ClientPixels + (UINTN) Row * Window->ClientWidth, (UINTN) ContentWidth * sizeof(UINT32));
                continue;
            }

            for (UINT32 Column = 0; Column < ContentWidth; Column++)
            {
                UINT32 SourceX = (UINT64) Column * Window->ClientWidth / ContentWidth;
                Window->Pixels[(UINTN) (Row + LWM_TITLE_HEIGHT) * Window->Stride + Column + 1] = Window->ClientPixels[(UINTN) SourceY * Window->ClientWidth + SourceX];
            }
        }

        Window->Dirty = FALSE;
        Manager->Surface = (TRUE_TYPE_SURFACE){Manager->Frame, Manager->Width, Manager->Height};

        return;
    }

    LwmText(Manager, gettext("Hello, LineOS!"), 24, 100, 32, 0x00FFFFFFU, Width - 20);
    LwmText(Manager, gettext("Drag the title bar to move."), 24, 143, 21, 0x00C4D0E3U, Width - 20);
    LwmText(Manager, gettext("Drag any edge or corner to resize."), 24, 175, 21, 0x00C4D0E3U, Width - 20);

    LWM_RECT Button = {24, (INT32) Height - 72, 152, 44};

    LwmFill(Manager, Button, Window->Hover ? 0x003B9FFFU : 0x000086FFU);
    LwmText(Manager, gettext("Click me"), 38, Height - 42, 23, 0x00FFFFFFU, 166);

    char   Digits[11];
    char   Reversed[10];
    UINT32 Count = 0;
    UINT32 Value = Window->Clicks;

    do
    {
        Reversed[Count++] = '0' + Value % 10;
        Value /= 10;
    } while (Value != 0);

    for (UINT32 Index = 0; Index < Count; Index++)
    {
        Digits[Index] = Reversed[Count - Index - 1];
    }

    Digits[Count] = 0;

    LwmText(Manager, Digits, 196, Height - 42, 23, 0x0068CB61U, Width - 20);
    LwmFill(Manager, (LWM_RECT){(INT32) Width - 14, (INT32) Height - 14, 12, 12}, 0x0050E3C2U);

    Window->Dirty = FALSE;
    Manager->Surface = (TRUE_TYPE_SURFACE){Manager->Frame, Manager->Width, Manager->Height};
}

STATIC INT32 LwmClamp(INT32 Value, INT32 Minimum, INT32 Maximum)
{
    return Value < Minimum ? Minimum : Value > Maximum ? Maximum : Value;
}

VOID LwmPointer(LWM *Manager, CONST POINTER_INFO *Pointer)
{
    BOOLEAN Pressed = (Pointer->Buttons & 1) != 0;
    BOOLEAN Clicked = Pressed && (Manager->PreviousButtons & 1) == 0;

    Manager->PreviousButtons = Pointer->Buttons;

    if (!Pressed)
    {
        Manager->Grab = -1;
    }

    if (Manager->PointerCapture == 0 && LwmDockPointer(Manager, Pointer, Clicked))
    {
        return;
    }

    if (Manager->Grab >= 0 && Pressed)
    {
        LWM_WINDOW *Window = &Manager->Windows[Manager->Grab];
        LWM_RECT    Previous = Window->Rect;

        if (Manager->Resizing)
        {
            INT32    DX = (INT32) Pointer->X - Manager->GrabX;
            INT32    DY = (INT32) Pointer->Y - Manager->GrabY;
            LWM_RECT Base = Manager->GrabRect;
            INT32    Left = Base.X;
            INT32    Top = Base.Y;
            INT32    Right = Base.X + (INT32) Base.Width;
            INT32    Bottom = Base.Y + (INT32) Base.Height;

            if ((Manager->ResizeEdges & 1U) != 0)
            {
                Left = LwmClamp(Base.X + DX, 0, Right - LWM_MIN_WIDTH);
            }

            if ((Manager->ResizeEdges & 2U) != 0)
            {
                Right = LwmClamp(Base.X + (INT32) Base.Width + DX, Left + LWM_MIN_WIDTH, Manager->Width);
            }

            if ((Manager->ResizeEdges & 4U) != 0)
            {
                Top = LwmClamp(Base.Y + DY, 0, Bottom - LWM_MIN_HEIGHT);
            }

            if ((Manager->ResizeEdges & 8U) != 0)
            {
                Bottom = LwmClamp(Base.Y + (INT32) Base.Height + DY, Top + LWM_MIN_HEIGHT, Manager->Height - LWM_DOCK_HEIGHT);
            }

            Window->Rect = (LWM_RECT){Left, Top, (UINT32) (Right - Left), (UINT32) (Bottom - Top)};
            Window->Dirty = Window->Dirty || memcmp(&Window->Rect, &Previous, sizeof(Previous)) != 0;
        }
        else if (!Window->Maximized)
        {
            Window->Rect.X = LwmClamp((INT32) Pointer->X - Manager->GrabX, 0, Manager->Width - Window->Rect.Width);
            Window->Rect.Y = LwmClamp((INT32) Pointer->Y - Manager->GrabY, 0, Manager->Height - LWM_DOCK_HEIGHT - Window->Rect.Height);
        }

        Manager->Dirty = Manager->Dirty || memcmp(&Previous, &Window->Rect, sizeof(Previous)) != 0;

        return;
    }

    INT32 Hit = -1;

    for (UINT32 Index = Manager->WindowCount; Index != 0; Index--)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index - 1];

        if (!Window->Minimized && LwmContains(Window->Rect, Pointer))
        {
            UINT32  X = Pointer->X - Window->Rect.X;
            UINT32  Y = Pointer->Y - Window->Rect.Y;
            UINT32  Inset = LwmRoundInset(Y, Window->Rect.Height, Window->Maximized ? 0 : 12);
            BOOLEAN Resize = Clicked && !Window->Maximized && (X < 6 || X >= Window->Rect.Width - 6 || Y < 6 || Y >= Window->Rect.Height - 6);

            if (!Resize && (X < Inset || X >= Window->Rect.Width - Inset))
            {
                continue;
            }

            Hit = (INT32) Index - 1;
            break;
        }
    }

    if (Clicked && Hit < 0)
    {
        Manager->ShellFocused = TRUE;
        Manager->Dirty = TRUE;

        for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
        {
            Manager->Windows[Index].Dirty = TRUE;
        }
    }

    if (Clicked && Hit >= 0)
    {
        Manager->ShellFocused = FALSE;
        LWM_WINDOW Focused = Manager->Windows[Hit];

        memmove(Manager->Windows + Hit, Manager->Windows + Hit + 1, (Manager->WindowCount - (UINT32) Hit - 1) * sizeof(Focused));
        Manager->Windows[Manager->WindowCount - 1] = Focused;
        Hit = (INT32) Manager->WindowCount - 1;

        for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
        {
            Manager->Windows[Index].Dirty = TRUE;
        }

        Manager->Dirty = TRUE;

        LWM_WINDOW *Window = &Manager->Windows[Hit];
        INT32       X = (INT32) Pointer->X - Window->Rect.X;
        INT32       Y = (INT32) Pointer->Y - Window->Rect.Y;

        if (LwmContains((LWM_RECT){Window->Rect.X + (INT32) Window->Rect.Width - 38, Window->Rect.Y + 8, 28, 28}, Pointer))
        {
            if (Window->Endpoint != 0 && (Window->ClientFlags & LOS_WINDOW_PERSISTENT) != 0)
            {
                Window->Minimized = TRUE;
            }
            else
            {
                if (Window->Endpoint != 0)
                {
                    LwmClientEvent(Manager, Window, LOS_WINDOW_CLOSE, NULL, 0);
                }

                LwmRemoveWindow(Manager, (UINT32) Hit);
            }

            return;
        }

        if (Y >= 8 && Y < 36 && X >= (INT32) Window->Rect.Width - 110 && X < (INT32) Window->Rect.Width - 82)
        {
            Window->Minimized = TRUE;
            Manager->Dirty = TRUE;
            return;
        }

        if (Y >= 8 && Y < 36 && X >= (INT32) Window->Rect.Width - 74 && X < (INT32) Window->Rect.Width - 46)
        {
            LwmMaximize(Manager, Window);
            return;
        }

        UINT32 Edges = 0;

        if (X < 6)
        {
            Edges |= 1;
        }

        if ((UINT32) X >= Window->Rect.Width - 6)
        {
            Edges |= 2;
        }

        if (Y < 6)
        {
            Edges |= 4;
        }

        if ((UINT32) Y >= Window->Rect.Height - 6)
        {
            Edges |= 8;
        }

        if (Edges != 0 && !Window->Maximized)
        {
            Manager->Grab = Hit;
            Manager->GrabX = (INT32) Pointer->X;
            Manager->GrabY = (INT32) Pointer->Y;
            Manager->GrabRect = Window->Rect;
            Manager->ResizeEdges = Edges;
            Manager->Resizing = TRUE;
        }
        else if ((UINT32) Y < LWM_TITLE_HEIGHT)
        {
            Manager->Grab = Hit;
            Manager->GrabX = X;
            Manager->GrabY = Y;
            Manager->Resizing = FALSE;
        }

        else if (Window->Endpoint == 0 && X >= 24 && X < 176 && Y >= (INT32) Window->Rect.Height - 72 && Y < (INT32) Window->Rect.Height - 28)
        {
            Window->Clicks++;
        }
        if (Window->Endpoint != 0 && Manager->Grab < 0 && Y >= (INT32) LWM_TITLE_HEIGHT)
        {
            Manager->PointerCapture = Window->Endpoint;
        }
    }

    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index];

        if (Window->Endpoint != 0)
        {
            continue;
        }

        LWM_RECT Button = {Window->Rect.X + 24, Window->Rect.Y + (INT32) Window->Rect.Height - 72, 152, 44};
        BOOLEAN  Hover = (INT32) Index == Hit && LwmContains(Button, Pointer);

        if (Window->Hover != Hover)
        {
            Window->Hover = Hover;
            Window->Dirty = TRUE;
            Manager->Dirty = TRUE;
        }
    }
    LWM_WINDOW *Target = Hit >= 0 && (UINT32) Hit < Manager->WindowCount ? &Manager->Windows[Hit] : Manager->Shell.HasFrame ? &Manager->Shell : NULL;

    if (Clicked && Target == &Manager->Shell)
    {
        Manager->PointerCapture = Manager->Shell.Endpoint;
    }

    if (Manager->PointerCapture == Manager->Shell.Endpoint && Manager->Shell.Endpoint != 0)
    {
        Target = &Manager->Shell;
    }
    else if (Manager->PointerCapture != 0)
    {
        for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
        {
            if (Manager->Windows[Index].Endpoint == Manager->PointerCapture)
            {
                Target = &Manager->Windows[Index];
                break;
            }
        }
    }

    BOOLEAN Sent = TRUE;

    if (Target != NULL && Target->Endpoint != 0)
    {
        if (Pointer->X != Manager->LastPointer.X || Pointer->Y != Manager->LastPointer.Y || Pointer->Buttons != Manager->LastPointer.Buttons || Pointer->Wheel != Manager->LastPointer.Wheel)
        {
            Sent = LwmClientEvent(Manager, Target, LOS_WINDOW_POINTER, Pointer, 0);
        }
    }

    if (Sent)
    {
        Manager->LastPointer = *Pointer;

        if (!Pressed)
        {
            Manager->PointerCapture = 0;
        }
    }
}

VOID LwmMaximize(LWM *Manager, LWM_WINDOW *Window)
{
    if (Window->Maximized)
    {
        Window->Rect = Window->Restore;
    }
    else
    {
        Window->Restore = Window->Rect;
        Window->Rect = (LWM_RECT){0, 0, Manager->Width, Manager->Height - LWM_DOCK_HEIGHT};
    }

    Window->Maximized = !Window->Maximized;
    Window->Dirty = TRUE;
    Manager->Dirty = TRUE;
}
