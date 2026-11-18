// UserPrograms/UserRescue/UserRescue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <locale.h>

#include <LOSLibC/TypeInfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <UserDB/UserDB.h>

#define USER_RESCUE_LINE_SIZE     256U
#define USER_RESCUE_PASSWORD_SIZE 128U

STATIC BOOLEAN UserRescueReadLine(char *Line, UINTN Capacity)
{
    UINTN Length = 0;
    UINT8 Byte;

    if (Line == NULL || Capacity < 2)
    {
        return FALSE;
    }

    Line[0] = 0;

    for (;;)
    {
        if (read(0, &Byte, 1) != 1)
        {
            return FALSE;
        }

        if (Byte == '\n' || Byte == '\r')
        {
            Line[Length] = 0;
            write(1, "\n", 1);

            return TRUE;
        }

        if (Byte == '\b' || Byte == 127)
        {
            if (Length > 0)
            {
                Length--;
                Line[Length] = 0;
                write(1, "\b \b", 3);
            }

            continue;
        }

        if (Byte >= 32 && Byte < 127 && Length + 1 < Capacity)
        {
            Line[Length++] = (char) Byte;
            Line[Length] = 0;
            write(1, &Byte, 1);
        }
    }
}

STATIC VOID UserRescueHelp(VOID)
{
    puts(gettext("Available commands:"));
    puts(gettext("  help              Show available commands"));
    puts(gettext("  status            Show recovery status"));
    puts(gettext("  adduser USERNAME  Create user account"));
    puts(gettext("  deluser USERNAME  Delete user account"));
    puts(gettext("  exit              Return to Login"));
}

STATIC VOID UserRescueStatus(VOID)
{
    puts(gettext("UserRescue: recovery environment active"));
    puts(gettext("UserRescue: use adduser root to initialize a missing account database"));
}

INT32 main(VOID)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    char Line[USER_RESCUE_LINE_SIZE];

    puts(gettext("LineOS User Rescue"));
    puts(gettext("Type 'help' for available commands."));

    for (;;)
    {
        printf("UserRescue> ");

        if (!UserRescueReadLine(Line, sizeof(Line)))
        {
            return 1;
        }

        if (Line[0] == 0)
        {
            continue;
        }

        if (strcmp(Line, "help") == 0)
        {
            UserRescueHelp();
        }
        else if (strcmp(Line, "status") == 0)
        {
            UserRescueStatus();
        }
        else if (strncmp(Line, "adduser ", 8) == 0)
        {
            CONST char *Username = Line + 8;

            if (*Username == 0)
            {
                puts(gettext("Usage: adduser USERNAME"));
            }
            else
            {
                UserDBAddCommand(Username);
            }
        }
        else if (strncmp(Line, "deluser ", 8) == 0)
        {
            CONST char *Username = Line + 8;

            if (*Username == 0)
            {
                puts(gettext("Usage: deluser USERNAME"));
            }
            else
            {
                UserDBDeleteCommand(Username);
            }
        }
        else if (strcmp(Line, "exit") == 0)
        {
            puts(gettext("UserRescue: returning to Login"));

            pid_t Child = fork();

            if (Child == 0)
            {
                char *Arguments[] = {"/sbin/Login.lix", NULL};
                char *Environment[] = {NULL};

                execve(Arguments[0], Arguments, Environment);
                puts(gettext("UserRescue: Login execve failed"));

                return 127;
            }

            if (Child < 0)
            {
                puts(gettext("UserRescue: Login fork failed"));
            }
            else
            {
                return 0;
            }
        }
        else
        {
            puts(gettext("UserRescue: unknown command"));
        }
    }
}