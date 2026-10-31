// UserPrograms/System/lwm/Source/Lwm.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Lwm.h>
#include <locale.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

STATIC LWM Manager;

INT32 main(VOID)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    INT32 Result = 1;

    if (!LwmOpen(&Manager))
    {
        INT32 Error = errno;

        LwmClose(&Manager);
        errno = Error;
        perror("lwm");

        return Result;
    }

    Manager.Running = TRUE;

    UINT64  NextFrame = 0;
    UINT64  StartupTime = 0;
    BOOLEAN KeyWaiting = FALSE;
    UINT8   WaitingKey = 0;

    while (Manager.Running)
    {
        if (LwmIPC(&Manager) < 0)
        {
            goto Cleanup;
        }

        POINTER_INFO Pointer;

        if (ioctl(Manager.TTY, LOS_TIO_GET_POINTER, &Pointer) < 0)
        {
            goto Cleanup;
        }

        LwmPointer(&Manager, &Pointer);

        INT32 Pending = 0;

        if (ioctl(Manager.TTY, FIONREAD, &Pending) < 0)
        {
            goto Cleanup;
        }

        for (UINT32 Batch = 0; Batch < 16 && (KeyWaiting || Pending > 0); Batch++)
        {
            if (!KeyWaiting)
            {
                if (read(Manager.TTY, &WaitingKey, 1) != 1)
                {
                    goto Cleanup;
                }

                Pending--;
            }

            KeyWaiting = !LwmKeyboard(&Manager, WaitingKey);

            if (KeyWaiting)
            {
                break;
            }
        }

        while (waitpid(-1, NULL, WNOHANG) > 0)
        {
        }

        if (!Manager.Running)
        {
            break;
        }

        struct timespec Clock;

        if (clock_gettime(CLOCK_MONOTONIC, &Clock) < 0)
        {
            goto Cleanup;
        }

        UINT64       Now = (UINT64) Clock.tv_sec * 1000000000ULL + Clock.tv_nsec;
        CONST UINT64 Period = 1000000000ULL / 120U;

        if (StartupTime == 0)
        {
            StartupTime = Now;
        }

        if (!Manager.InitialShell && Now - StartupTime > 5000000000ULL)
        {
            errno = ETIMEDOUT;
            goto Cleanup;
        }

        if (Manager.Dirty && Now >= NextFrame)
        {
            if (LwmCompose(&Manager) < 0)
            {
                goto Cleanup;
            }

            NextFrame = Now + Period;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &Clock) < 0)
        {
            goto Cleanup;
        }

        Now = (UINT64) Clock.tv_sec * 1000000000ULL + Clock.tv_nsec;
        UINT64 Wait = NextFrame > Now ? NextFrame - Now : 1000000ULL;

        if (!Manager.Dirty)
        {
            Wait = Period;
        }

        if (Wait > 1000000ULL)
        {
            Wait = 1000000ULL;
        }

        struct timespec Delay = {0, (long) Wait};

        if (nanosleep(&Delay, NULL) < 0 && errno != EINTR)
        {
            goto Cleanup;
        }
    }

    if (Result != 130)
    {
        Result = 0;
    }

Cleanup:
{
    INT32 Error = errno;

    LwmClose(&Manager);

    if (Result == 1)
    {
        errno = Error;
        perror("lwm");
    }
}

    return Result;
}
