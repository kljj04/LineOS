// Libraries/Source/UserDB/DeleteCommand.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <stdio.h>

INT32 UserDBDeleteCommand(CONST char *Username)
{
    USER_DB_RESULT Status = UserDBDelete(Username);

    printf("DelUser: %s\n", UserDBResultText(Status));

    return Status == USER_DB_OK ? 0 : (Status == USER_DB_COMMITTED_CLEANUP ? 2 : 1);
}
