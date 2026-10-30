// UserPrograms/Shell/Commands/sudo/Source/Sudo.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Sudo.h>
#include <UserDB/Argon2.h>
#include <unistd.h>
#include <stdio.h>

INT32 main(INT32 Argc, char **Argv)
{
    if (Argc < 2)
    {
        puts("Usage: sudo COMMAND [ARGUMENT ...]");
        return 1;
    }
    if (geteuid() != 0)
    {
        puts("sudo: executable must be owned by root with mode 4755");
        return 1;
    }
    char Username[USER_DB_NAME_MAX + 1];
    USER_DB_ACCOUNT Account;
    if (UserDBLookupUID(getuid(), Username, &Account) != USER_DB_OK)
    {
        puts("sudo: cannot identify calling user");
        return 1;
    }
    if (!SudoAllowed(Username))
    {
        CryptoWipe(&Account, sizeof(Account));
        puts("sudo: user is not allowed by /etc/sudoers");
        return 1;
    }
    if (!SudoAuthenticate(Username, &Account))
    {
        puts("sudo: authentication failed");
        return 1;
    }
    return SudoExecute(Argc - 1, Argv + 1, Username);
}
