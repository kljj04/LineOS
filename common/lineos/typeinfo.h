// typeinfo.h
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
#endif
