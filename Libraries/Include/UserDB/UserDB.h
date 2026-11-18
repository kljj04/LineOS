// Libraries/Include/UserDB/UserDB.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

#define USER_DB_NAME_MAX       32U
#define USER_DB_PASSWORD_MAX   127U
#define USER_DB_SALT_SIZE      16U
#define USER_DB_SALT_TEXT_SIZE 33U
#define USER_DB_HASH_TEXT_SIZE 104U
#define USER_DB_HASH_PREFIX    "argon2id$v=19$m=65536,t=3,p=1$"

typedef enum
{
    USER_DB_OK,
    USER_DB_INVALID_NAME,
    USER_DB_UNAVAILABLE,
    USER_DB_INVALID,
    USER_DB_EXISTS,
    USER_DB_NOT_FOUND,
    USER_DB_ROOT_REQUIRED,
    USER_DB_ROOT_PROTECTED,
    USER_DB_LIMIT,
    USER_DB_LOCKED,
    USER_DB_IO,
    USER_DB_CLEANUP,
    USER_DB_COMMITTED_CLEANUP
} USER_DB_RESULT;

BOOLEAN        UserDBValidateName(CONST char *Username);
BOOLEAN        UserDBValidatePasswordFields(CONST char *Salt, CONST char *Hash);
BOOLEAN        UserDBHashPassword(CONST char *Password, char Salt[USER_DB_SALT_TEXT_SIZE], char Hash[USER_DB_HASH_TEXT_SIZE]);
BOOLEAN        UserDBVerifyPassword(CONST char *Password, CONST char *Salt, CONST char *Hash);
USER_DB_RESULT UserDBAdd(CONST char *Username, CONST char *Salt, CONST char *Hash);
USER_DB_RESULT UserDBDelete(CONST char *Username);
CONST char    *UserDBResultText(USER_DB_RESULT Result);
INT32          UserDBAddCommand(CONST char *Username);
INT32          UserDBDeleteCommand(CONST char *Username);

typedef struct
{
    UINT32 UID;
    UINT32 GID;
    char   Salt[USER_DB_SALT_TEXT_SIZE];
    char   Hash[USER_DB_HASH_TEXT_SIZE];
} USER_DB_ACCOUNT;

USER_DB_RESULT UserDBCheck(VOID);
USER_DB_RESULT UserDBLookup(CONST char *Username, USER_DB_ACCOUNT *Account);

USER_DB_RESULT UserDBLookupUID(UINT32 UID, char Username[USER_DB_NAME_MAX + 1], USER_DB_ACCOUNT *Account);
