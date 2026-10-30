// UserPrograms/DelUser/DelUser.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <stdio.h>
#include <stdlib.h>

INT32 main(INT32 Argc, char **Argv)
{
    if (Argc != 2 || Argv == NULL || Argv[1] == NULL)
    {
        puts("Usage: deluser USERNAME");
        return 1;
    }
    return (INT8) UserDBDeleteCommand(Argv[1]);
}
