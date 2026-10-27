// UserPrograms/AddUser/AddUser.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <stdio.h>
#include <stdlib.h>

INT32 _Start(INT32 Argc, char **Argv)
{
    if (Argc != 2 || Argv == NULL || Argv[1] == NULL)
    {
        puts("Usage: adduser USERNAME");
        exit(1);
    }
    exit((INT8) UserDBAddCommand(Argv[1]));
}
