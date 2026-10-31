// UserPrograms/System/lwm/Source/LwmIPC.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Lwm.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

VOID LwmRemoveWindow(LWM *Manager, UINT32 Index)
{
    LWM_WINDOW *Window = &Manager->Windows[Index];

    LwmDamage(Manager, (LWM_RECT){Window->Rect.X - 12, Window->Rect.Y - 12, Window->Rect.Width + 24, Window->Rect.Height + 28});

    if (Manager->PointerCapture == Window->Endpoint)
    {
        Manager->PointerCapture = 0;
    }

    if (Window->ClientPixels != NULL)
    {
        munmap(Window->ClientPixels, Window->ClientBytes);
        munmap(Window->PendingPixels, Window->ClientBytes);
    }

    munmap(Window->Pixels, Window->Bytes);
    memmove(Window, Window + 1, (Manager->WindowCount - Index - 1) * sizeof(*Window));
    Manager->WindowCount--;
    Manager->Grab = -1;
    Manager->Dirty = TRUE;

    for (UINT32 Other = 0; Other < Manager->WindowCount; Other++)
    {
        Manager->Windows[Other].Dirty = TRUE;
    }
}

BOOLEAN LwmClientEvent(LWM *Manager, LWM_WINDOW *Window, UINT32 Type, CONST POINTER_INFO *Pointer, UINT32 Key)
{
    LOS_WINDOW_MESSAGE Event = {.Version = LOS_WINDOW_VERSION, .Type = Type, .Key = Key};

    if (Pointer != NULL)
    {
        BOOLEAN Background = Window == &Manager->Shell;
        INT64   X = (INT64) Pointer->X - Window->Rect.X - (Background ? 0 : 1);
        INT64   Y = (INT64) Pointer->Y - Window->Rect.Y - (Background ? 0 : LWM_TITLE_HEIGHT);
        UINT32  Width = Window->Rect.Width - (Background ? 0 : 2);
        UINT32  Height = Window->Rect.Height - (Background ? 0 : LWM_TITLE_HEIGHT + 1);

        if (X < 0 || Y < 0 || X >= Width || Y >= Height)
        {
            Event.X = Window->ClientWidth;
            Event.Y = Window->ClientHeight;
        }
        else
        {
            Event.X = (UINT32) X * Window->ClientWidth / Width;
            Event.Y = (UINT32) Y * Window->ClientHeight / Height;
        }
        Event.Buttons = Pointer->Buttons;
        Event.Wheel = Pointer->Wheel;
    }

    return LOSIPCSend(Manager->Endpoint, Window->Endpoint, &Event, sizeof(Event)) == 0;
}

VOID LwmRemoveShell(LWM *Manager)
{
    LWM_WINDOW *Shell = &Manager->Shell;

    if (Shell->ClientPixels != NULL)
    {
        munmap(Shell->ClientPixels, Shell->ClientBytes);
        munmap(Shell->PendingPixels, Shell->ClientBytes);
    }

    memset(Shell, 0, sizeof(*Shell));
    Manager->InitialShell = FALSE;
    Manager->ShellFocused = TRUE;
    Manager->Dirty = TRUE;
}

STATIC VOID LwmClientCreate(LWM *Manager, UINT64 Endpoint, UINT32 UID, CONST LOS_WINDOW_MESSAGE *Message)
{
    LOS_WINDOW_MESSAGE Reply = {.Version = LOS_WINDOW_VERSION, .Type = LOS_WINDOW_CLOSE};
    BOOLEAN            Background = (Message->Flags & LOS_WINDOW_BACKGROUND) != 0;

    if (Message->Width == 0 || Message->Height == 0 || Message->Width > 2560 || Message->Height > 1440 || Message->Title[63] != 0 || (Message->Flags & ~(LOS_WINDOW_PERSISTENT | LOS_WINDOW_BACKGROUND)) != 0 || (Background && (UID != 0 || Manager->Shell.Endpoint != 0)))
    {
        LOSIPCSend(Manager->Endpoint, Endpoint, &Reply, sizeof(Reply));
        return;
    }

    LWM_WINDOW *Window;

    if (Background)
    {
        Window = &Manager->Shell;
    }
    else
    {
        if (!LwmCreateWindow(Manager))
        {
            LOSIPCSend(Manager->Endpoint, Endpoint, &Reply, sizeof(Reply));
            return;
        }

        Window = &Manager->Windows[Manager->WindowCount - 1];
    }

    Window->ClientBytes = (UINT64) Message->Width * Message->Height * sizeof(UINT32);
    Window->ClientPixels = mmap(NULL, Window->ClientBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Window->PendingPixels = mmap(NULL, Window->ClientBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (Window->ClientPixels == MAP_FAILED || Window->PendingPixels == MAP_FAILED)
    {
        if (Window->ClientPixels != MAP_FAILED)
        {
            munmap(Window->ClientPixels, Window->ClientBytes);
        }

        if (Window->PendingPixels != MAP_FAILED)
        {
            munmap(Window->PendingPixels, Window->ClientBytes);
        }

        Window->ClientPixels = NULL;
        Window->PendingPixels = NULL;

        if (Background)
        {
            memset(Window, 0, sizeof(*Window));
        }
        else
        {
            LwmRemoveWindow(Manager, Manager->WindowCount - 1);
        }

        LOSIPCSend(Manager->Endpoint, Endpoint, &Reply, sizeof(Reply));
        return;
    }

    Window->Endpoint = Endpoint;
    Window->ClientWidth = Message->Width;
    Window->ClientHeight = Message->Height;
    Window->PendingFirst = Message->Height;
    Window->ClientFlags = Message->Flags;
    strcpy(Window->Title, Message->Title);

    if (Background)
    {
        Window->Rect = (LWM_RECT){0, 0, Manager->Width, Manager->Height - LWM_DOCK_HEIGHT};
        Manager->ShellFocused = TRUE;
    }
    else
    {
        UINT32 Width = Message->Width + 2;
        UINT32 Height = Message->Height + LWM_TITLE_HEIGHT + 1;

        if (Width < LWM_MIN_WIDTH)
        {
            Width = LWM_MIN_WIDTH;
        }

        if (Height < LWM_MIN_HEIGHT)
        {
            Height = LWM_MIN_HEIGHT;
        }

        Window->Rect.Width = Width < Window->Stride ? Width : Window->Stride;
        Window->Rect.Height = Height < Window->CapacityHeight ? Height : Window->CapacityHeight;
        Window->Rect.X = (Manager->Width - Window->Rect.Width) / 2;
        Window->Rect.Y = (Manager->Height - LWM_DOCK_HEIGHT - Window->Rect.Height) / 2;
        Manager->ShellFocused = FALSE;
    }

    Reply.Type = LOS_WINDOW_CREATED;
    LOSIPCSend(Manager->Endpoint, Endpoint, &Reply, sizeof(Reply));
}

INT32 LwmIPC(LWM *Manager)
{
    if (Manager->Shell.Endpoint != 0 && LOSIPCAlive(Manager->Endpoint, Manager->Shell.Endpoint) < 0)
    {
        LwmRemoveShell(Manager);
    }

    for (UINT32 Index = 0; Index < Manager->WindowCount;)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index];

        if (Window->Endpoint != 0 && LOSIPCAlive(Manager->Endpoint, Window->Endpoint) < 0)
        {
            LwmRemoveWindow(Manager, Index);
        }
        else
        {
            Index++;
        }
    }

    BOOLEAN Visible = FALSE;

    for (UINT32 Index = 0; Index < Manager->WindowCount; Index++)
    {
        Visible = Visible || !Manager->Windows[Index].Minimized;
    }

    if (!Visible && !Manager->ShellFocused)
    {
        Manager->ShellFocused = TRUE;
        Manager->Dirty = TRUE;
    }

    for (UINT32 Batch = 0; Batch < 32; Batch++)
    {
        LOS_IPC_REQUEST Sender;
        INT64           Size = LOSIPCReceive(Manager->Endpoint, Manager->MessageBuffer, LOS_IPC_MESSAGE_SIZE, &Sender);

        if (Size < 0)
        {
            return errno == EAGAIN ? 0 : -1;
        }

        if (Size < sizeof(LOS_WINDOW_MESSAGE))
        {
            continue;
        }

        LOS_WINDOW_MESSAGE *Message = (LOS_WINDOW_MESSAGE *) Manager->MessageBuffer;

        if (Message->Version != LOS_WINDOW_VERSION)
        {
            continue;
        }

        LWM_WINDOW *Window = Manager->Shell.Endpoint == Sender.Peer ? &Manager->Shell : NULL;
        UINT32      Index = 0;

        for (; Index < Manager->WindowCount; Index++)
        {
            if (Window != NULL)
            {
                break;
            }

            if (Manager->Windows[Index].Endpoint == Sender.Peer)
            {
                Window = &Manager->Windows[Index];
                break;
            }
        }

        if (Message->Type == LOS_WINDOW_CREATE && Window == NULL && Size == sizeof(*Message))
        {
            LwmClientCreate(Manager, Sender.Peer, Sender.UID, Message);
            continue;
        }

        if (Window == NULL)
        {
            continue;
        }

        if (Message->Type == LOS_WINDOW_DESTROY)
        {
            if (Window == &Manager->Shell)
            {
                LwmRemoveShell(Manager);
            }
            else
            {
                LwmRemoveWindow(Manager, Index);
            }
        }
        else if (Message->Type == LOS_WINDOW_PIXELS)
        {
            UINT64 Bytes = (UINT64) Message->Width * Message->Height * sizeof(UINT32);

            if (Message->X == 0 && Message->Width == Window->ClientWidth && Message->Y < Window->ClientHeight && Message->Height <= Window->ClientHeight - Message->Y && Bytes == (UINT64) Size - sizeof(*Message))
            {
                memcpy(Window->PendingPixels + (UINTN) Message->Y * Window->ClientWidth, Manager->MessageBuffer + sizeof(*Message), Bytes);

                if (Message->Y < Window->PendingFirst)
                {
                    Window->PendingFirst = Message->Y;
                }

                if (Message->Y + Message->Height > Window->PendingLast)
                {
                    Window->PendingLast = Message->Y + Message->Height;
                }
            }
        }
        else if (Message->Type == LOS_WINDOW_PRESENT && Size == sizeof(*Message))
        {
            if (Window->PendingFirst < Window->PendingLast)
            {
                UINTN Offset = (UINTN) Window->PendingFirst * Window->ClientWidth;
                UINTN Bytes = (UINTN) (Window->PendingLast - Window->PendingFirst) * Window->ClientWidth * sizeof(UINT32);

                memcpy(Window->ClientPixels + Offset, Window->PendingPixels + Offset, Bytes);

                if (Window == &Manager->Shell)
                {
                    LwmDamage(Manager, (LWM_RECT){0, (INT32) Window->PendingFirst, Manager->Width, Window->PendingLast - Window->PendingFirst});
                }
            }

            Window->PendingFirst = Window->ClientHeight;
            Window->PendingLast = 0;
            Window->HasFrame = TRUE;
            Window->Dirty = TRUE;
            Manager->Dirty = TRUE;

            if ((Window->ClientFlags & LOS_WINDOW_BACKGROUND) != 0)
            {
                Manager->InitialShell = TRUE;
            }
        }
    }

    return 0;
}

BOOLEAN LwmKeyboard(LWM *Manager, UINT8 Key)
{
    if (Manager->ShellFocused && Manager->Shell.Endpoint != 0)
    {
        return ioctl(Manager->TTY, LOS_TIO_INJECT_CLI, &Key) == 0 || errno != EAGAIN;
    }

    for (UINT32 Index = Manager->WindowCount; Index != 0; Index--)
    {
        LWM_WINDOW *Window = &Manager->Windows[Index - 1];

        if (Window->Minimized)
        {
            continue;
        }

        if (Window->Endpoint != 0)
        {
            LOS_WINDOW_MESSAGE Event = {.Version = LOS_WINDOW_VERSION, .Type = LOS_WINDOW_KEY, .Key = Key};

            return LOSIPCSend(Manager->Endpoint, Window->Endpoint, &Event, sizeof(Event)) == 0 || errno != EAGAIN;
        }

        break;
    }

    Manager->ShellFocused = TRUE;

    if (Manager->Shell.Endpoint != 0)
    {
        return ioctl(Manager->TTY, LOS_TIO_INJECT_CLI, &Key) == 0 || errno != EAGAIN;
    }

    return TRUE;
}
