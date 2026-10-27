// UserPrograms/UserRescue/UserRescue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <UserDB/UserDB.h>
#include <spawn.h>

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
    puts("Available commands:");
    puts("  help              Show available commands");
    puts("  status            Show recovery status");
    puts("  adduser USERNAME  Create user account");
    puts("  deluser USERNAME  Delete user account");
    puts("  exit              Return to Login");
}

STATIC VOID UserRescueStatus(VOID)
{
    puts("UserRescue: recovery environment active");
    puts("UserRescue: use adduser root to initialize a missing account database");
}

INT32 _Start(VOID)
{
    char Line[USER_RESCUE_LINE_SIZE];

    puts("LineOS User Rescue");
    puts("Type 'help' for available commands.");

    for (;;)
    {
        printf("UserRescue> ");

        if (!UserRescueReadLine(Line, sizeof(Line)))
        {
            exit(1);
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
                puts("Usage: adduser USERNAME");
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
                puts("Usage: deluser USERNAME");
            }
            else
            {
                UserDBDeleteCommand(Username);
            }
        }
        else if (strcmp(Line, "exit") == 0)
        {
            puts("UserRescue: returning to Login");
            if (spawn("/sbin/Login.lix") < 0)
            {
                puts("UserRescue: failed to launch Login");
            }
            else
            {
                exit(0);
            }
        }
        else
        {
            puts("UserRescue: unknown command");
        }
    }
}