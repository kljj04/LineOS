// Libraries/Include/UserDB/Argon2.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

VOID  CryptoWipe(VOID *Secret, UINTN Size);
INT32 CryptoVerify32(CONST UINT8 A[32], CONST UINT8 B[32]);

typedef struct
{

    UINT64 Hash[8];
    UINT64 InputOffset[2];
    UINT64 Input[16];
    UINTN  InputIdx;
    UINTN  HashSize;
} CRYPTO_BLAKE2B_CONTEXT;

VOID CryptoBlake2bInit(CRYPTO_BLAKE2B_CONTEXT *Ctx, UINTN HashSize);
VOID CryptoBlake2bKeyedInit(CRYPTO_BLAKE2B_CONTEXT *Ctx, UINTN HashSize, CONST UINT8 *Key, UINTN KeySize);
VOID CryptoBlake2bUpdate(CRYPTO_BLAKE2B_CONTEXT *Ctx, CONST UINT8 *Message, UINTN MessageSize);
VOID CryptoBlake2bFinal(CRYPTO_BLAKE2B_CONTEXT *Ctx, UINT8 *Hash);

#define CRYPTO_ARGON2_D  0
#define CRYPTO_ARGON2_I  1
#define CRYPTO_ARGON2_ID 2

typedef struct
{
    UINT32 Algorithm;
    UINT32 NbBlocks;
    UINT32 NbPasses;
    UINT32 NbLanes;
} CRYPTO_ARGON2_CONFIG;

typedef struct
{
    CONST UINT8 *Pass;
    CONST UINT8 *Salt;
    UINT32       PassSize;
    UINT32       SaltSize;
} CRYPTO_ARGON2_INPUTS;

typedef struct
{
    CONST UINT8 *Key;
    CONST UINT8 *Ad;
    UINT32       KeySize;
    UINT32       AdSize;
} CRYPTO_ARGON2_EXTRAS;

extern CONST CRYPTO_ARGON2_EXTRAS CryptoArgon2NoExtras;

VOID CryptoArgon2(UINT8 *Hash, UINT32 HashSize, VOID *WorkArea, CRYPTO_ARGON2_CONFIG Config, CRYPTO_ARGON2_INPUTS Inputs, CRYPTO_ARGON2_EXTRAS Extras);
