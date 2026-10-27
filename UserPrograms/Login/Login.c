// UserPrograms/Login/Login.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <UserDB/Argon2.h>
#include <spawn.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

STATIC INT32 LoginReadLine(char *Buffer, UINTN Capacity, BOOLEAN Echo)
{
    UINTN   Length = 0;
    BOOLEAN Invalid = FALSE;
    UINT8   Byte;
    Buffer[0] = 0;

    for (;;)
    {
        if (read(0, &Byte, 1) != 1)
        {
            return -1;
        }
        if (Byte == '\r')
        {
            continue;
        }
        if (Byte == '\n')
        {
            write(1, "\n", 1);
            Buffer[Length] = 0;
            return Invalid ? 0 : 1;
        }
        if (Byte == '\b' || Byte == 127)
        {
            if (Length != 0 && !Invalid)
            {
                Buffer[--Length] = 0;
                if (Echo)
                {
                    write(1, "\b \b", 3);
                }
            }
            continue;
        }
        if (Byte < 32 || Byte > 126 || Length + 1 >= Capacity)
        {
            Invalid = TRUE;
            continue;
        }
        if (Invalid)
        {
            continue;
        }
        Buffer[Length++] = (char) Byte;
        Buffer[Length] = 0;
        if (Echo)
        {
            write(1, &Byte, 1);
        }
    }
}

STATIC VOID NORETURN LoginRescue(VOID)
{
    puts("Login: account database unavailable or invalid; entering UserRescue");
    if (spawn("/sys/rescue/UserRescue.lix") < 0)
    {
        puts("Login: failed to launch UserRescue");
        exit(1);
    }
    exit(0);
}

INT32 _Start(VOID)
{
    char            Username[USER_DB_NAME_MAX + 1];
    char            Password[USER_DB_PASSWORD_MAX + 1];
    USER_DB_ACCOUNT Account;

    puts("LineOS Login");
    if (UserDBCheck() != USER_DB_OK)
    {
        LoginRescue();
    }

    for (;;)
    {
        CryptoWipe(Username, sizeof(Username));
        CryptoWipe(Password, sizeof(Password));
        CryptoWipe(&Account, sizeof(Account));
        printf("Username: ");
        INT32 Input = LoginReadLine(Username, sizeof(Username), TRUE);
        if (Input < 0)
        {
            exit(1);
        }
        if (Input == 0 || !UserDBValidateName(Username))
        {
            puts("Login: invalid username");
            continue;
        }

        printf("Password: ");
        Input = LoginReadLine(Password, sizeof(Password), FALSE);
        if (Input < 0)
        {
            CryptoWipe(Password, sizeof(Password));
            exit(1);
        }
        if (Input == 0)
        {
            CryptoWipe(Password, sizeof(Password));
            puts("Login: incorrect username or password");
            continue;
        }

        USER_DB_RESULT Status = UserDBLookup(Username, &Account);
        BOOLEAN        Found = Status == USER_DB_OK;
        if (Status == USER_DB_NOT_FOUND)
        {
            Status = UserDBLookup("root", &Account);
        }
        if (Status != USER_DB_OK)
        {
            CryptoWipe(Password, sizeof(Password));
            CryptoWipe(&Account, sizeof(Account));
            LoginRescue();
        }

        BOOLEAN Authenticated = UserDBVerifyPassword(Password, Account.Salt, Account.Hash);
        CryptoWipe(Password, sizeof(Password));
        if (!Found || !Authenticated)
        {
            CryptoWipe(&Account, sizeof(Account));
            puts("Login: incorrect username or password");
            continue;
        }

        UINT32 UID = Account.UID;
        UINT32 GID = Account.GID;
        CryptoWipe(&Account, sizeof(Account));
        CryptoWipe(Username, sizeof(Username));
        if (setgid(GID) != 0 || setuid(UID) != 0)
        {
            puts("Login: failed to establish user credentials");
            exit(1);
        }
        if (spawn("/bin/Shell.lix") < 0)
        {
            puts("Login: failed to launch Shell");
            exit(1);
        }
        exit(0);
    }
}
