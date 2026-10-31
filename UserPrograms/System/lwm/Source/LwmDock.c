// UserPrograms/System/lwm/Source/LwmDock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Lwm.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>

STATIC UINT32 LwmDockSlot(LWM *Manager)
{
    UINT32 Slot = Manager->WindowCount != 0 ? (Manager->Width - 336) / Manager->WindowCount : 132;

    return Slot > 132 ? 132 : Slot;
}

VOID LwmDock(LWM *Manager)
{
    Manager->Surface = (TRUE_TYPE_SURFACE){Manager->Frame, Manager->Width, Manager->Height};

    LwmFill(Manager, (LWM_RECT){0, (INT32) Manager->Height - LWM_DOCK_HEIGHT, Manager->Width, LWM_DOCK_HEIGHT}, 0x00232B38U);
    LwmFill(Manager, (LWM_RECT){12, (INT32) Manager->Height - 54, 96, 44}, Manager->ShellFocused ? 0x003D5874U : 0x00303E50U);
    LwmText(Manager, "Shell", 24, Manager->Height - 24, 21, 0x00FFFFFFU, 102);
    LwmFill(Manager, (LWM_RECT){116, (INT32) Manager->Height - 54, 120, 44}, 0x00354860U);
    LwmText(Manager, "TestGUI +", 126, Manager->Height - 24, 21, 0x00FFFFFFU, 232);
    LwmFill(Manager, (LWM_RECT){(INT32) Manager->Width - 80, (INT32) Manager->Height - 54, 68, 44}, 0x00423038U);
    LwmText(Manager, gettext("Exit"), Manager->Width - 70, Manager->Height - 24, 21, 0x00FFB4B4U, Manager->Width - 16);

    UINT32 Slot = LwmDockSlot(Manager);

    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index];
        UINT32      X = 248 + Index * Slot;

        LwmFill(Manager, (LWM_RECT){(INT32) X, (INT32) Manager->Height - 54, Slot - 8, 44}, Window->Minimized ? 0x002D3340U : !Manager->ShellFocused && Index + 1 == Manager->WindowCount ? 0x003D5874U : 0x00303E50U);
        LwmText(Manager, Window->Endpoint != 0 ? Window->Title : "Demo", X + 6, Manager->Height - 24, 21, 0x00E0E8F4U, X + Slot - 10);
    }
}

BOOLEAN LwmDockPointer(LWM *Manager, CONST POINTER_INFO *Pointer, BOOLEAN Clicked)
{
    if (Pointer->Y < Manager->Height - LWM_DOCK_HEIGHT)
    {
        return FALSE;
    }

    if (!Clicked)
    {
        return TRUE;
    }

    if (Pointer->X >= Manager->Width - 80)
    {
        Manager->Running = FALSE;
        return TRUE;
    }

    if (Pointer->X >= 12 && Pointer->X < 108)
    {
        Manager->ShellFocused = TRUE;
    }
    else if (Pointer->X >= 116 && Pointer->X < 236)
    {
        pid_t Child = fork();

        if (Child == 0)
        {
            close(Manager->TTY);
            close(Manager->FB);

            char *Arguments[] = {"/bin/lsh/TestGUI.lix", NULL};

            execve(Arguments[0], Arguments, environ);
            exit(127);
        }
    }
    else if (Pointer->X >= 248 && Manager->WindowCount != 0)
    {
        UINT32 Index = (Pointer->X - 248) / LwmDockSlot(Manager);

        if (Index < Manager->WindowCount)
        {
            LWM_WINDOW Window = Manager->Windows[Index];

            Window.Minimized = FALSE;
            Window.Dirty = TRUE;
            memmove(Manager->Windows + Index, Manager->Windows + Index + 1, (Manager->WindowCount - Index - 1) * sizeof(Window));
            Manager->Windows[Manager->WindowCount - 1] = Window;
            Manager->ShellFocused = FALSE;
        }
    }

    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        Manager->Windows[Index].Dirty = TRUE;
    }

    Manager->Dirty = TRUE;

    return TRUE;
}
