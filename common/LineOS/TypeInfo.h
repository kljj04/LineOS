// common/lineos/typeinfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#define LINEOS_TYPEINFO_H

#ifndef LINEOS_KERNEL_BUILD
#include <Protocol/GraphicsOutput.h>
#include <Uefi.h>
#endif

#undef CONST
#undef NULL
#undef TRUE
#undef FALSE
#undef STATIC
#undef EXTERN
#undef INLINE
#undef VOLATILE
#undef PACKED
#undef MS_ABI
#undef SYSV_ABI
#undef ASM

#define CONST    const
#define NULL     ((VOID *) 0)
#define TRUE     ((BOOLEAN) 1)
#define FALSE    ((BOOLEAN) 0)
#define STATIC   static
#define EXTERN   extern
#define INLINE   inline
#define VOLATILE volatile
#define PACKED   __attribute__((packed))
#define MS_ABI   __attribute__((ms_abi))
#define SYSV_ABI __attribute__((sysv_abi))
#define ASM      __asm__ volatile
#define NORETURN __attribute__((noreturn))

#ifdef LINEOS_KERNEL_BUILD
typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long long UINT64;
typedef unsigned __int128  UINT128;
typedef signed char        INT8;
typedef signed short       INT16;
typedef signed int         INT32;
typedef signed long long   INT64;
typedef signed __int128    INT128;
typedef unsigned char      CHAR8;
typedef unsigned short     CHAR16;
typedef unsigned long long UINTN;
typedef signed long long   INTN;
typedef float              FLOAT32;
typedef double             FLOAT64;
typedef unsigned char      BOOLEAN;
typedef void               VOID;

#define UINT8_MAX   255U
#define UINT16_MAX  65535U
#define UINT32_MAX  4294967295U
#define UINT64_MAX  18446744073709551615ULL
#define INT8_MAX    127
#define INT16_MAX   32767
#define INT32_MAX   2147483647
#define INT64_MAX   9223372036854775807LL
#define INT8_MIN    (-128)
#define INT16_MIN   (-32768)
#define INT32_MIN   (-2147483647 - 1)
#define INT64_MIN   (-9223372036854775807LL - 1)
#define UINT128_MAX ((UINT128) 340282366920938463463374607431768211455)
#define INT128_MAX  ((INT128) 170141183460469231731687303715884105727)
#define INT128_MIN  (-INT128_MAX - 1)
#define UINTN_MAX   18446744073709551615ULL
#define INTN_MAX    9223372036854775807LL
#define INTN_MIN    (-9223372036854775807LL - 1)
#endif
