// Scripts/Validation/SecureExec/Hash.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB.h>
#include <stdio.h>

int main(void)
{
    char Salt[USER_DB_SALT_TEXT_SIZE];
    char Hash[USER_DB_HASH_TEXT_SIZE];
    if (!UserDBHashPassword("FixturePassword42", Salt, Hash))
    {
        return 1;
    }
    printf("root:0:0:%s:%s\ntester:1000:1000:%s:%s\n", Salt, Hash, Salt, Hash);
    return 0;
}
