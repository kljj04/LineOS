// Libraries/Source/UserDB/Codec.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <string.h>

STATIC BOOLEAN UserDBHex(CONST char *Text, UINTN Length)
{
    if (Text == NULL || strlen(Text) != Length)
    {
        return FALSE;
    }

    for (UINTN Index = 0; Index < Length; Index++)
    {
        char Character = Text[Index];

        if (!((Character >= '0' && Character <= '9') || (Character >= 'a' && Character <= 'f')))
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLEAN UserDBValidatePasswordFields(CONST char *Salt, CONST char *Hash)
{
    UINTN PrefixLength = sizeof(USER_DB_HASH_PREFIX) - 1;

    return Hash != NULL && strncmp(Hash, USER_DB_HASH_PREFIX, PrefixLength) == 0 && UserDBHex(Salt, 32) && UserDBHex(Hash + PrefixLength, 64);
}
