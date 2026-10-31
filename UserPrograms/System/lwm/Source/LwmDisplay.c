// UserPrograms/System/lwm/Source/LwmDisplay.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Lwm.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

STATIC VOID *LwmMap(UINT64 Bytes)
{
    VOID *Memory = mmap(NULL, Bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    return Memory == MAP_FAILED ? NULL : Memory;
}

BOOLEAN LwmOpen(LWM *Manager)
{
    Manager->TTY = -1;
    Manager->FB = -1;
    Manager->Grab = -1;
    Manager->TTY = open("/dev/lsh1", O_RDWR);
    Manager->FB = open("/dev/fb0", O_RDWR);

    if (Manager->TTY < 0 || Manager->FB < 0)
    {
        return FALSE;
    }

    FB_VAR_SCREENINFO Variable;
    FB_FIX_SCREENINFO Fixed;

    if (ioctl(Manager->FB, FBIOGET_VSCREENINFO, &Variable) < 0 || ioctl(Manager->FB, FBIOGET_FSCREENINFO, &Fixed) < 0)
    {
        return FALSE;
    }

    if (Variable.XRes < 640 || Variable.YRes < 480 || Variable.XRes > 4096 || Variable.YRes > 2160 || Variable.BitsPerPixel != 32 || Variable.Red.Offset != 16 || Variable.Green.Offset != 8 || Variable.Blue.Offset != 0 || Variable.Red.Length != 8 || Variable.Green.Length != 8 || Variable.Blue.Length != 8 || Fixed.LineLength != (UINT64) Variable.XRes * 4 || Fixed.SmemLen != (UINT64) Fixed.LineLength * Variable.YRes)
    {
        errno = ENOTSUP;

        return FALSE;
    }

    Manager->Width = Variable.XRes;
    Manager->Height = Variable.YRes;
    Manager->Bytes = Fixed.SmemLen;
    Manager->Mapped = mmap(NULL, Manager->Bytes, PROT_READ | PROT_WRITE, MAP_SHARED, Manager->FB, 0);

    if (Manager->Mapped == MAP_FAILED)
    {
        Manager->Mapped = NULL;

        return FALSE;
    }

    Manager->Frame = LwmMap(Manager->Bytes);
    Manager->Desktop = LwmMap(Manager->Bytes);
    Manager->PresentBuffer = LwmMap(Manager->Bytes);
    Manager->FontWork = LwmMap(LWM_FONT_WORK_SIZE);

    if (Manager->Frame == NULL || Manager->Desktop == NULL || Manager->PresentBuffer == NULL || Manager->FontWork == NULL)
    {
        errno = ENOMEM;

        return FALSE;
    }

    if (LOSAssetAcquire("Fonts/JetBrainsMono", &Manager->FontAsset) != 0 || LOSAssetAcquire("Fonts/Pretendard", &Manager->FallbackAsset) != 0)
    {
        return FALSE;
    }

    Manager->Surface = (TRUE_TYPE_SURFACE){Manager->Frame, Manager->Width, Manager->Height};

    Manager->Allocator = (TTF_ALLOCATOR){LwmAllocate, LwmFree, Manager};

    if (!TrueTypeInit(&Manager->Allocator, &Manager->Surface) || !TrueTypeRegisterFont(&Manager->Font, Manager->FontAsset.Data) || !TrueTypeRegisterFont(&Manager->Fallback, Manager->FallbackAsset.Data))
    {
        errno = EINVAL;

        return FALSE;
    }

    if (ioctl(Manager->TTY, LOS_TIO_GRAPHICS_BEGIN) < 0)
    {
        return FALSE;
    }

    Manager->Graphics = TRUE;

    INT64 Endpoint = LOSIPCCreate("lwm");

    if (Endpoint < 0)
    {
        return FALSE;
    }

    Manager->Endpoint = (UINT64) Endpoint;
    Manager->MessageBuffer = LwmMap(LOS_IPC_MESSAGE_SIZE);

    if (Manager->MessageBuffer == NULL || ioctl(Manager->TTY, LOS_TIO_SET_COMPOSITOR, &Manager->Endpoint) < 0)
    {
        return FALSE;
    }

    POINTER_INFO Pointer;

    if (ioctl(Manager->TTY, LOS_TIO_GET_POINTER, &Pointer) == 0)
    {
        Manager->PreviousButtons = Pointer.Buttons;
    }

    LwmDesktop(Manager);
    Manager->Dirty = TRUE;

    return TRUE;
}

VOID LwmClose(LWM *Manager)
{
    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index];

        if (Window->Endpoint != 0 && (Window->ClientFlags & LOS_WINDOW_PERSISTENT) == 0)
        {
            LwmClientEvent(Manager, Window, LOS_WINDOW_CLOSE, NULL, 0);
        }
    }

    if (Manager->Graphics)
    {
        ioctl(Manager->TTY, LOS_TIO_GRAPHICS_END);
        Manager->Graphics = FALSE;
    }

    while (Manager->WindowCount != 0)
    {
        LwmRemoveWindow(Manager, Manager->WindowCount - 1);
    }

    LwmRemoveShell(Manager);

    if (Manager->Endpoint != 0)
    {
        LOSIPCClose(Manager->Endpoint);
    }

    if (Manager->MessageBuffer != NULL)
    {
        munmap(Manager->MessageBuffer, LOS_IPC_MESSAGE_SIZE);
    }

    if (Manager->FallbackAsset.Data != NULL)
    {
        LOSAssetRelease(&Manager->FallbackAsset);
    }

    if (Manager->FontAsset.Data != NULL)
    {
        LOSAssetRelease(&Manager->FontAsset);
    }

    if (Manager->FontWork != NULL)
    {
        munmap(Manager->FontWork, LWM_FONT_WORK_SIZE);
    }

    if (Manager->PresentBuffer != NULL)
    {
        munmap(Manager->PresentBuffer, Manager->Bytes);
    }

    if (Manager->Desktop != NULL)
    {
        munmap(Manager->Desktop, Manager->Bytes);
    }

    if (Manager->Frame != NULL)
    {
        munmap(Manager->Frame, Manager->Bytes);
    }

    if (Manager->Mapped != NULL)
    {
        munmap(Manager->Mapped, Manager->Bytes);
    }

    if (Manager->FB >= 0)
    {
        close(Manager->FB);
    }

    if (Manager->TTY >= 0)
    {
        close(Manager->TTY);
    }
}
