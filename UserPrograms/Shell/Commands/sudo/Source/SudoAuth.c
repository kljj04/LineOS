// UserPrograms/Shell/Commands/sudo/Source/SudoAuth.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <Sudo.h>
#include <UserDB/Argon2.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

BOOLEAN SudoAllowed(CONST char *Username)
{
    INT32 FD = open("/etc/sudoers", O_RDONLY);

    if (FD < 0)
    {
        return FALSE;
    }

    struct stat Info;

    if (fstat(FD, &Info) < 0 || !S_ISREG(Info.st_mode) || Info.st_uid != 0 || (Info.st_mode & 0022) != 0)
    {
        close(FD);

        return FALSE;
    }

    char    Line[USER_DB_NAME_MAX + 2];
    UINTN   Length = 0;
    BOOLEAN Overflow = FALSE;
    BOOLEAN Allowed = FALSE;
    char    Byte;

    for (;;)
    {
        ssize_t Count = read(FD, &Byte, 1);

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count < 0)
        {
            Allowed = FALSE;
            break;
        }

        if (Count == 0 || Byte == '\n')
        {
            Line[Length] = 0;

            if (!Overflow && strcmp(Line, Username) == 0)
            {
                Allowed = TRUE;
            }

            Length = 0;
            Overflow = FALSE;

            if (Count == 0)
            {
                break;
            }

            continue;
        }

        if (Byte == 0 || Length + 1 >= sizeof(Line))
        {
            Overflow = TRUE;
        }
        else
        {
            Line[Length++] = Byte;
        }
    }

    if (close(FD) != 0)
    {
        Allowed = FALSE;
    }

    return Allowed;
}

BOOLEAN SudoAuthenticate(CONST char *Username, USER_DB_ACCOUNT *Account)
{
    char    Password[USER_DB_PASSWORD_MAX + 1] = {0};
    UINTN   Length = 0;
    BOOLEAN Valid = TRUE;
    BOOLEAN Complete = FALSE;
    UINT8   Byte;

    printf(gettext("[sudo] password for %s: "), Username);

    while (read(0, &Byte, 1) == 1)
    {
        if (Byte == '\r')
        {
            continue;
        }

        if (Byte == '\n')
        {
            Complete = TRUE;
            break;
        }

        if (Byte == '\b' || Byte == 127)
        {
            if (Length != 0 && Valid)
            {
                Password[--Length] = 0;
            }

            continue;
        }

        if (Byte < 32 || Byte > 126 || Length + 1 >= sizeof(Password))
        {
            Valid = FALSE;
        }

        if (Valid)
        {
            Password[Length++] = (char) Byte;
        }
    }

    putchar('\n');

    BOOLEAN Result = FALSE;

    if (Valid && Complete)
    {
        Result = UserDBVerifyPassword(Password, Account->Salt, Account->Hash);
    }

    CryptoWipe(Password, sizeof(Password));
    CryptoWipe(Account, sizeof(*Account));

    return Result;
}
