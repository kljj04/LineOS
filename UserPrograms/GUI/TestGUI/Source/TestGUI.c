// UserPrograms/GUI/TestGUI/Source/TestGUI.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <locale.h>

#include <TestGUI.h>
#include <Asset.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>

INT32 main(VOID)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    INT32 Windowed = TestGUIWindowed();

    if (Windowed != -2)
    {
        return Windowed;
    }

    TEST_GUI          GUI = {0};
    FB_VAR_SCREENINFO Variable;
    FB_FIX_SCREENINFO Fixed;
    LOS_ASSET         FontAsset = {0};
    INT32             Result = 1;
    BOOLEAN           Graphics = FALSE;
    INT32             TTY = open("/dev/lsh1", O_RDWR);
    INT32             FB = open("/dev/fb0", O_RDWR);

    if (TTY < 0 || FB < 0)
    {
        puts(gettext("TestGUI: cannot open devices"));

        goto Cleanup;
    }

    if (ioctl(FB, FBIOGET_VSCREENINFO, &Variable) < 0 || ioctl(FB, FBIOGET_FSCREENINFO, &Fixed) < 0)
    {
        puts(gettext("TestGUI: cannot query framebuffer"));

        goto Cleanup;
    }

    if (Variable.XRes < 640 || Variable.YRes < 480 || Variable.BitsPerPixel != 32 || Variable.Red.Offset != 16 || Variable.Green.Offset != 8 || Variable.Blue.Offset != 0 || Fixed.LineLength != (UINT64) Variable.XRes * 4 || Fixed.SmemLen != (UINT64) Fixed.LineLength * Variable.YRes)
    {
        puts(gettext("TestGUI: unsupported framebuffer"));

        goto Cleanup;
    }

    GUI.Width = Variable.XRes;
    GUI.Height = Variable.YRes;
    GUI.Pixels = mmap(NULL, Fixed.SmemLen, PROT_READ | PROT_WRITE, MAP_SHARED, FB, 0);

    if (GUI.Pixels == MAP_FAILED)
    {
        GUI.Pixels = NULL;

        goto Cleanup;
    }

    GUI.Work = mmap(NULL, TEST_GUI_ALLOCATOR_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (GUI.Work == MAP_FAILED)
    {
        GUI.Work = NULL;

        goto Cleanup;
    }

    if (LOSAssetAcquire("Fonts/JetBrainsMono", &FontAsset) < 0)
    {
        puts(gettext("TestGUI: cannot acquire font"));

        goto Cleanup;
    }

    TRUE_TYPE_SURFACE Surface = {GUI.Pixels, GUI.Width, GUI.Height};
    TTF_ALLOCATOR     Allocator = {TestGUIAllocate, TestGUIFree, &GUI};

    if (!TrueTypeInit(&Allocator, &Surface) || !TrueTypeRegisterFont(&GUI.Font, FontAsset.Data))
    {
        puts(gettext("TestGUI: cannot initialize font"));

        goto Cleanup;
    }

    GUI.Window.Width = GUI.Width < 800 ? GUI.Width - 64 : 736;
    GUI.Window.Height = 400;
    GUI.Window.X = (GUI.Width - GUI.Window.Width) / 2;
    GUI.Window.Y = (GUI.Height - GUI.Window.Height) / 2;
    GUI.Close = (TEST_GUI_RECT){GUI.Window.X + GUI.Window.Width - 56, GUI.Window.Y + 8, 44, 40};
    GUI.Button = (TEST_GUI_RECT){GUI.Window.X + 32, GUI.Window.Y + 265, 176, 58};

    if (ioctl(TTY, LOS_TIO_GRAPHICS_BEGIN) < 0)
    {
        puts(gettext("TestGUI: display is busy"));

        goto Cleanup;
    }

    Graphics = TRUE;

    POINTER_INFO Pointer;
    UINT32       PreviousButtons = 0;

    if (ioctl(TTY, LOS_TIO_GET_POINTER, &Pointer) == 0)
    {
        PreviousButtons = Pointer.Buttons;
    }

    TestGUIDraw(&GUI);

    if (ioctl(FB, LOS_FBIO_FLUSH) < 0)
    {
        goto Cleanup;
    }

    for (;;)
    {
        if (ioctl(TTY, LOS_TIO_GET_POINTER, &Pointer) < 0)
        {
            break;
        }

        BOOLEAN Clicked = (Pointer.Buttons & 1) != 0 && (PreviousButtons & 1) == 0;

        PreviousButtons = Pointer.Buttons;

        if (Clicked && TestGUIContains(&GUI.Close, &Pointer))
        {
            Result = 0;
            break;
        }

        BOOLEAN Hover = TestGUIContains(&GUI.Button, &Pointer);
        BOOLEAN Redraw = Hover != GUI.Hover;

        GUI.Hover = Hover;

        if (Clicked && Hover)
        {
            GUI.Clicks++;
            Redraw = TRUE;
        }

        if (Redraw)
        {
            TestGUIDraw(&GUI);

            if (ioctl(FB, LOS_FBIO_FLUSH) < 0)
            {
                break;
            }
        }

        struct timespec Delay = {0, 20000000};

        if (nanosleep(&Delay, NULL) < 0)
        {
            break;
        }
    }

Cleanup:
    if (Graphics)
    {
        ioctl(TTY, LOS_TIO_GRAPHICS_END);
    }

    if (FontAsset.Data != NULL)
    {
        LOSAssetRelease(&FontAsset);
    }

    if (GUI.Work != NULL)
    {
        munmap(GUI.Work, TEST_GUI_ALLOCATOR_SIZE);
    }

    if (GUI.Pixels != NULL)
    {
        munmap(GUI.Pixels, Fixed.SmemLen);
    }

    if (FB >= 0)
    {
        close(FB);
    }

    if (TTY >= 0)
    {
        close(TTY);
    }

    return Result;
}
