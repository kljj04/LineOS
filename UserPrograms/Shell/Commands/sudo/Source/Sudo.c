// UserPrograms/Shell/Commands/sudo/Source/Sudo.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLocale.h>

#include <locale.h>

#include <Sudo.h>
#include <Argon2.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <LOS.h>
#include <Syscall/Security/Sudo.h>

INT32 main(INT32 Argc, char **Argv)
{
    if (LOSLocaleRefresh() == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    if (Argc < 2)
    {
        puts(LOSGettext("Usage: sudo COMMAND [ARGUMENT ...] | sudo -k"));

        return 1;
    }

    if (geteuid() != 0)
    {
        puts(LOSGettext("sudo: executable must be owned by root with mode 4755"));

        return 1;
    }

    if (Argc == 2 && strcmp(Argv[1], "-k") == 0)
    {
        INT64 Result = LOSSudoCache(SUDO_CACHE_CLEAR);

        return Result < 0 ? 1 : 0;
    }

    char            Username[USER_DB_NAME_MAX + 1];
    USER_DB_ACCOUNT Account;

    if (UserDBLookupUID(getuid(), Username, &Account) != USER_DB_OK)
    {
        puts(LOSGettext("sudo: cannot identify calling user"));

        return 1;
    }

    if (!SudoAllowed(Username))
    {
        CryptoWipe(&Account, sizeof(Account));
        puts(LOSGettext("sudo: user is not allowed by /etc/sudoers"));

        return 1;
    }

    INT64 Cached = LOSSudoCache(SUDO_CACHE_CHECK);

    if (Cached != 1)
    {
        if (!SudoAuthenticate(Username, &Account))
        {
            puts(LOSGettext("sudo: authentication failed"));

            return 1;
        }

        LOSSudoCache(SUDO_CACHE_RECORD);
    }
    else
    {
        CryptoWipe(&Account, sizeof(Account));
    }

    return SudoExecute(Argc - 1, Argv + 1, Username);
}
