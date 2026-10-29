// UserPrograms/Login/Login.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <UserDB/Argon2.h>
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

STATIC INT32 LoginRescue(VOID)
{
    puts("Login: account database unavailable or invalid; entering UserRescue");

    pid_t Child = fork();

    if (Child == 0)
    {
        char *Arguments[] = {"/sys/rescue/UserRescue.lix", NULL};
        char *Environment[] = {NULL};

        execve(Arguments[0], Arguments, Environment);
        puts("Login: UserRescue execve failed");

        return 127;
    }

    if (Child < 0)
    {
        puts("Login: UserRescue fork failed");

        return 1;
    }

    return 0;
}

INT32 main(VOID)
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
            return 1;
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
            return 1;
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
        char UserEnvironment[5 + sizeof(Username)] = "USER=";
        strcpy(UserEnvironment + 5, Username);
        CryptoWipe(Username, sizeof(Username));
        if (setgid(GID) != 0 || setuid(UID) != 0)
        {
            puts("Login: failed to establish user credentials");
            return 1;
        }
        pid_t Child = fork();
        if (Child == 0)
        {
            char *Arguments[] = {"/bin/lsh.lix", NULL};
            char *Environment[] = {UserEnvironment, "PATH=/bin/lsh", NULL};
            execve(Arguments[0], Arguments, Environment);
            puts("Login: shell execve failed");
            return 127;
        }
        if (Child < 0)
        {
            puts("Login: shell fork failed");
            return 1;
        }
        return 0;
    }
}
