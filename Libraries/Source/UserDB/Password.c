// Libraries/Source/UserDB/Password.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <UserDB/UserDB.h>
#include <UserDB/Argon2.h>
#include <sys/random.h>
#include <string.h>
#include <sys/mman.h>

#define PASSWORD_WORK_SIZE (65536U * 128U * sizeof(UINT64))
STATIC CONST char HexDigits[] = "0123456789abcdef";

STATIC INT32 PasswordHexValue(char Character)
{
    if (Character >= '0' && Character <= '9')
    {
        return Character - '0';
    }
    if (Character >= 'a' && Character <= 'f')
    {
        return Character - 'a' + 10;
    }
    return -1;
}

STATIC VOID PasswordEncode(CONST UINT8 *Bytes, UINTN Count, char *Text)
{
    for (UINTN Index = 0; Index < Count; Index++)
    {
        Text[Index * 2] = HexDigits[Bytes[Index] >> 4];
        Text[Index * 2 + 1] = HexDigits[Bytes[Index] & 15];
    }
    Text[Count * 2] = 0;
}

STATIC BOOLEAN PasswordDecode(CONST char *Text, UINT8 *Bytes, UINTN Count)
{
    if (Text == NULL || strlen(Text) != Count * 2)
    {
        return FALSE;
    }
    for (UINTN Index = 0; Index < Count; Index++)
    {
        INT32 High = PasswordHexValue(Text[Index * 2]);
        INT32 Low = PasswordHexValue(Text[Index * 2 + 1]);
        if (High < 0 || Low < 0)
        {
            return FALSE;
        }
        Bytes[Index] = (UINT8) ((High << 4) | Low);
    }
    return TRUE;
}

STATIC BOOLEAN PasswordDerive(CONST char *Password, CONST UINT8 Salt[16], UINT8 Hash[32])
{
    UINTN Length = Password == NULL ? 0 : strlen(Password);
    if (Length < 8 || Length > USER_DB_PASSWORD_MAX)
    {
        return FALSE;
    }
    VOID *PasswordWork = mmap(NULL, PASSWORD_WORK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (PasswordWork == MAP_FAILED)
    {
        return FALSE;
    }

    CRYPTO_ARGON2_CONFIG Config = {CRYPTO_ARGON2_ID, 65536, 3, 1};
    CRYPTO_ARGON2_INPUTS Inputs = {(CONST UINT8 *) Password, Salt, (UINT32) Length, 16};
    CryptoArgon2(Hash, 32, PasswordWork, Config, Inputs, CryptoArgon2NoExtras);
    CryptoWipe(PasswordWork, PASSWORD_WORK_SIZE);

    return munmap(PasswordWork, PASSWORD_WORK_SIZE) == 0;
}

BOOLEAN UserDBHashPassword(CONST char *Password, char Salt[USER_DB_SALT_TEXT_SIZE], char Hash[USER_DB_HASH_TEXT_SIZE])
{
    UINT8   SaltBytes[16] = {0};
    UINT8   HashBytes[32] = {0};
    BOOLEAN Result = FALSE;
    if (Salt == NULL || Hash == NULL)
    {
        return FALSE;
    }
    Salt[0] = 0;
    Hash[0] = 0;
    if (getrandom(SaltBytes, sizeof(SaltBytes), 0) == sizeof(SaltBytes) && PasswordDerive(Password, SaltBytes, HashBytes))
    {
        PasswordEncode(SaltBytes, sizeof(SaltBytes), Salt);
        strcpy(Hash, USER_DB_HASH_PREFIX);
        PasswordEncode(HashBytes, sizeof(HashBytes), Hash + sizeof(USER_DB_HASH_PREFIX) - 1);
        Result = TRUE;
    }
    CryptoWipe(HashBytes, sizeof(HashBytes));
    CryptoWipe(SaltBytes, sizeof(SaltBytes));
    return Result;
}

BOOLEAN UserDBVerifyPassword(CONST char *Password, CONST char *Salt, CONST char *Hash)
{
    UINT8   SaltBytes[16];
    UINT8   Expected[32];
    UINT8   Actual[32] = {0};
    BOOLEAN Result = FALSE;
    if (UserDBValidatePasswordFields(Salt, Hash) && PasswordDecode(Salt, SaltBytes, 16) && PasswordDecode(Hash + sizeof(USER_DB_HASH_PREFIX) - 1, Expected, 32) && PasswordDerive(Password, SaltBytes, Actual))
    {
        Result = CryptoVerify32(Actual, Expected) == 0;
    }
    CryptoWipe(Actual, sizeof(Actual));
    CryptoWipe(Expected, sizeof(Expected));
    CryptoWipe(SaltBytes, sizeof(SaltBytes));
    return Result;
}
