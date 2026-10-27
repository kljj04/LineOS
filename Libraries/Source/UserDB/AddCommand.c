// Libraries/Source/UserDB/AddCommand.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <UserDB/Argon2.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

STATIC BOOLEAN AddUserReadPassword(char Password[USER_DB_PASSWORD_MAX + 1])
{
    UINTN   Length = 0;
    UINT8   Byte;
    BOOLEAN Invalid = FALSE;
    Password[0] = 0;
    for (;;)
    {
        if (read(0, &Byte, 1) != 1)
        {
            return FALSE;
        }
        if (Byte == '\r')
        {
            continue;
        }
        if (Byte == '\n')
        {
            write(1, "\n", 1);
            Password[Length] = 0;
            return !Invalid && Length >= 8;
        }
        if (Byte == '\b' || Byte == 127)
        {
            if (Length != 0)
            {
                Password[--Length] = 0;
            }
            continue;
        }
        if (Byte < 32 || Byte > 126 || Length == USER_DB_PASSWORD_MAX)
        {
            Invalid = TRUE;
            continue;
        }
        if (Invalid)
        {
            continue;
        }
        Password[Length++] = (char) Byte;
        Password[Length] = 0;
    }
}

INT32 UserDBAddCommand(CONST char *Username)
{
    char  Password[USER_DB_PASSWORD_MAX + 1] = {0};
    char  Confirmation[USER_DB_PASSWORD_MAX + 1] = {0};
    char  Salt[USER_DB_SALT_TEXT_SIZE] = {0};
    char  Hash[USER_DB_HASH_TEXT_SIZE] = {0};
    INT32 Result = 1;
    if (!UserDBValidateName(Username))
    {
        puts("AddUser: invalid username");
        return 1;
    }
    printf("New password for %s: ", Username);
    if (!AddUserReadPassword(Password))
    {
        puts("AddUser: password must contain 8 to 127 printable characters");
        goto Cleanup;
    }
    printf("Confirm password: ");
    if (!AddUserReadPassword(Confirmation) || strcmp(Password, Confirmation) != 0)
    {
        puts("AddUser: passwords do not match or confirmation is invalid");
        goto Cleanup;
    }
    puts("AddUser: deriving password hash...");
    if (!UserDBHashPassword(Password, Salt, Hash))
    {
        puts("AddUser: secure random source or password hashing unavailable");
        goto Cleanup;
    }
    CryptoWipe(Password, sizeof(Password));
    CryptoWipe(Confirmation, sizeof(Confirmation));
    USER_DB_RESULT Status = UserDBAdd(Username, Salt, Hash);
    printf("AddUser: %s\n", UserDBResultText(Status));
    Result = Status == USER_DB_OK ? 0 : (Status == USER_DB_COMMITTED_CLEANUP ? 2 : 1);
Cleanup:
    CryptoWipe(Password, sizeof(Password));
    CryptoWipe(Confirmation, sizeof(Confirmation));
    CryptoWipe(Hash, sizeof(Hash));
    CryptoWipe(Salt, sizeof(Salt));
    return Result;
}
