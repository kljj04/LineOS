// UserPrograms/GUI/TestGUI/Source/TestGUIWindow.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TestGUI.h>
#include <Asset.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>

INT32 TestGUIWindowed(VOID)
{
    TEST_GUI  GUI = {0};
    LOS_ASSET Font = {0};
    INT32     Result = 1;

    if (LOSWindowOpen(&GUI.Client, 0, "TestGUI", 720, 380, 0) < 0)
    {
        return errno == ENOENT ? -2 : 1;
    }

    GUI.Width = 720;
    GUI.Height = 380;
    GUI.Pixels = mmap(NULL, (UINTN) GUI.Width * GUI.Height * sizeof(UINT32), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    GUI.Work = mmap(NULL, TEST_GUI_ALLOCATOR_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (GUI.Pixels == MAP_FAILED || GUI.Work == MAP_FAILED || LOSAssetAcquire("Fonts/JetBrainsMono", &Font) < 0)
    {
        goto Cleanup;
    }

    TRUE_TYPE_SURFACE Surface = {GUI.Pixels, GUI.Width, GUI.Height};
    TTF_ALLOCATOR     Allocator = {TestGUIAllocate, TestGUIFree, &GUI};

    if (!TrueTypeInit(&Allocator, &Surface) || !TrueTypeRegisterFont(&GUI.Font, Font.Data))
    {
        goto Cleanup;
    }

    GUI.Window = (TEST_GUI_RECT){0, 0, GUI.Width, GUI.Height};
    GUI.Button = (TEST_GUI_RECT){32, 265, 176, 58};

    BOOLEAN Dirty = TRUE;
    UINT32  PreviousButtons = 0;

    for (;;)
    {
        LOS_WINDOW_MESSAGE Event;
        INT32              Received = LOSWindowPoll(&GUI.Client, &Event);

        if (Received < 0)
        {
            break;
        }

        if (Received > 0 && (Event.Type == LOS_WINDOW_CLOSE || (Event.Type == LOS_WINDOW_KEY && (Event.Key == 27 || Event.Key == 3))))
        {
            Result = Event.Type == LOS_WINDOW_KEY && Event.Key == 3 ? 130 : 0;
            break;
        }

        if (Received > 0 && Event.Type == LOS_WINDOW_POINTER)
        {
            POINTER_INFO Pointer = {Event.X, Event.Y, Event.Buttons, Event.Wheel};
            BOOLEAN      Hover = TestGUIContains(&GUI.Button, &Pointer);
            BOOLEAN      Clicked = (Pointer.Buttons & 1) != 0 && (PreviousButtons & 1) == 0;

            PreviousButtons = Pointer.Buttons;
            Dirty = Dirty || GUI.Hover != Hover;
            GUI.Hover = Hover;

            if (Hover && Clicked)
            {
                GUI.Clicks++;
                Dirty = TRUE;
            }
        }

        if (Dirty)
        {
            TestGUIDraw(&GUI);

            if (LOSWindowPresent(&GUI.Client, GUI.Pixels) < 0)
            {
                break;
            }

            Dirty = FALSE;
        }

        struct timespec Delay = {0, 8333333};

        nanosleep(&Delay, NULL);
    }

Cleanup:
    LOSWindowClose(&GUI.Client);

    if (Font.Data != NULL)
    {
        LOSAssetRelease(&Font);
    }

    if (GUI.Pixels != NULL && GUI.Pixels != MAP_FAILED)
    {
        munmap(GUI.Pixels, (UINTN) GUI.Width * GUI.Height * sizeof(UINT32));
    }

    if (GUI.Work != NULL && GUI.Work != MAP_FAILED)
    {
        munmap(GUI.Work, TEST_GUI_ALLOCATOR_SIZE);
    }

    return Result;
}
