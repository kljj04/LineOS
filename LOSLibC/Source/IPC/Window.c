// LOSLibC/Source/IPC/Window.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Window.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include <errno.h>

STATIC INT32 WindowSend(LOS_WINDOW *Window, CONST VOID *Message, UINT32 Size)
{
    for (UINT32 Attempt = 0; Attempt < 2000; Attempt++)
    {
        if (LOSIPCSend(Window->Endpoint, Window->Server, Message, Size) == 0)
        {
            return 0;
        }

        if (errno != EAGAIN)
        {
            return -1;
        }

        struct timespec Delay = {0, 1000000};

        nanosleep(&Delay, NULL);
    }

    errno = ETIMEDOUT;

    return -1;
}

VOID LOSWindowClose(LOS_WINDOW *Window)
{
    if (Window->Endpoint != 0)
    {
        LOS_WINDOW_MESSAGE Message = {.Version = LOS_WINDOW_VERSION, .Type = LOS_WINDOW_DESTROY};

        LOSIPCSend(Window->Endpoint, Window->Server, &Message, sizeof(Message));
        LOSIPCClose(Window->Endpoint);
    }

    if (Window->Transfer != NULL)
    {
        munmap(Window->Transfer, LOS_IPC_MESSAGE_SIZE);
    }

    if (Window->Previous != NULL)
    {
        munmap(Window->Previous, (UINT64) Window->Width * Window->Height * sizeof(UINT32));
    }

    memset(Window, 0, sizeof(*Window));
}

INT32 LOSWindowPoll(LOS_WINDOW *Window, LOS_WINDOW_MESSAGE *Event)
{
    if (Window == NULL || Event == NULL || Window->Endpoint == 0)
    {
        errno = EINVAL;
        return -1;
    }

    LOS_IPC_REQUEST Sender;
    INT64           Count = LOSIPCReceive(Window->Endpoint, Event, sizeof(*Event), &Sender);

    if (Count < 0)
    {
        return errno == EAGAIN && LOSIPCAlive(Window->Endpoint, Window->Server) >= 0 ? 0 : -1;
    }

    if (Count != sizeof(*Event) || Sender.Peer != Window->Server || Event->Version != LOS_WINDOW_VERSION)
    {
        return 0;
    }

    return 1;
}

INT32 LOSWindowOpen(LOS_WINDOW *Window, UINT64 Server, CONST char *Title, UINT32 Width, UINT32 Height, UINT32 Flags)
{
    if (Window == NULL || Title == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    memset(Window, 0, sizeof(*Window));

    if (Width == 0 || Height == 0 || Width > 2560 || Height > 1440 || strlen(Title) >= sizeof(((LOS_WINDOW_MESSAGE *) 0)->Title))
    {
        errno = EINVAL;

        return -1;
    }

    INT64 Found = Server != 0 ? (INT64) Server : LOSIPCLookup("lwm");

    if (Found < 0)
    {
        return -1;
    }

    Window->Server = (UINT64) Found;
    Found = LOSIPCCreate(NULL);

    if (Found < 0)
    {
        return -1;
    }

    Window->Endpoint = (UINT64) Found;
    Window->Width = Width;
    Window->Height = Height;
    Window->Transfer = mmap(NULL, LOS_IPC_MESSAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (Window->Transfer == MAP_FAILED)
    {
        Window->Transfer = NULL;
        goto Failed;
    }

    Window->Previous = mmap(NULL, (UINT64) Width * Height * sizeof(UINT32), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (Window->Previous == MAP_FAILED)
    {
        Window->Previous = NULL;
        goto Failed;
    }

    if (LOSIPCBind(Window->Endpoint, Window->Server) < 0)
    {
        goto Failed;
    }

    LOS_WINDOW_MESSAGE Message = {.Version = LOS_WINDOW_VERSION, .Type = LOS_WINDOW_CREATE, .Width = Width, .Height = Height, .Flags = Flags};

    strcpy(Message.Title, Title);

    if (WindowSend(Window, &Message, sizeof(Message)) < 0)
    {
        goto Failed;
    }

    for (UINT32 Attempt = 0; Attempt < 2000; Attempt++)
    {
        LOS_WINDOW_MESSAGE Event;
        INT32              Result = LOSWindowPoll(Window, &Event);

        if (Result < 0 || (Result > 0 && Event.Type == LOS_WINDOW_CLOSE))
        {
            errno = ECONNREFUSED;
            goto Failed;
        }

        if (Result > 0 && Event.Type == LOS_WINDOW_CREATED)
        {
            return 0;
        }

        struct timespec Delay = {0, 1000000};

        nanosleep(&Delay, NULL);
    }

    errno = ETIMEDOUT;

Failed:
{
    INT32 Error = errno;

    LOSWindowClose(Window);
    errno = Error;
}

    return -1;
}

INT32 LOSWindowPresentRows(LOS_WINDOW *Window, CONST UINT32 *Pixels, UINT32 Begin, UINT32 End)
{
    if (Window == NULL || Window->Endpoint == 0 || Window->Transfer == NULL || Window->Previous == NULL || Pixels == NULL || Window->Width == 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (Begin > End || End > Window->Height)
    {
        errno = EINVAL;
        return -1;
    }

    UINT32 First = Window->Presented ? Window->Height : 0;
    UINT32 Last = Window->Presented ? 0 : Window->Height;
    UINT32 RowBytes = Window->Width * sizeof(UINT32);

    if (Window->Presented)
    {
        for (UINT32 Row = Begin; Row < End; Row++)
        {
            UINTN Offset = (UINTN) Row * Window->Width;

            if (memcmp(Window->Previous + Offset, Pixels + Offset, RowBytes) != 0)
            {
                if (First == Window->Height)
                {
                    First = Row;
                }

                Last = Row + 1;
            }
        }
    }

    if (First >= Last)
    {
        return 0;
    }

    UINT32 Row = First;
    UINT32 Batch = (LOS_IPC_MESSAGE_SIZE - sizeof(LOS_WINDOW_MESSAGE)) / RowBytes;

    while (Row < Last)
    {
        UINT32 Rows = Last - Row;

        if (Rows > Batch)
        {
            Rows = Batch;
        }

        LOS_WINDOW_MESSAGE *Message = (LOS_WINDOW_MESSAGE *) Window->Transfer;

        memset(Message, 0, sizeof(*Message));
        Message->Version = LOS_WINDOW_VERSION;
        Message->Type = LOS_WINDOW_PIXELS;
        Message->Width = Window->Width;
        Message->Height = Rows;
        Message->Y = Row;

        memcpy(Window->Transfer + sizeof(*Message), Pixels + (UINTN) Row * Window->Width, (UINTN) Rows * RowBytes);

        if (WindowSend(Window, Message, sizeof(*Message) + Rows * RowBytes) < 0)
        {
            return -1;
        }

        Row += Rows;
    }

    LOS_WINDOW_MESSAGE Message = {.Version = LOS_WINDOW_VERSION, .Type = LOS_WINDOW_PRESENT};

    if (WindowSend(Window, &Message, sizeof(Message)) < 0)
    {
        return -1;
    }

    memcpy(Window->Previous + (UINTN) First * Window->Width, Pixels + (UINTN) First * Window->Width, (UINTN) (Last - First) * RowBytes);
    Window->Presented = TRUE;

    return 0;
}

INT32 LOSWindowPresent(LOS_WINDOW *Window, CONST UINT32 *Pixels)
{
    return LOSWindowPresentRows(Window, Pixels, 0, Window != NULL ? Window->Height : 0);
}
