// Common/Userspace/TypeInfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

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
#undef NORETURN
#undef ATOMIC_LOAD
#undef ATOMIC_STORE
#undef ATOMIC_EXCHANGE
#undef ATOMIC_FETCH_ADD
#undef ATOMIC_FETCH_SUB
#undef ATOMIC_COMPARE_EXCHANGE

#define CONST                                                                             const
#define NULL                                                                              ((VOID *) 0)
#define TRUE                                                                              ((BOOLEAN) 1)
#define FALSE                                                                             ((BOOLEAN) 0)
#define STATIC                                                                            static
#define EXTERN                                                                            extern
#define INLINE                                                                            inline
#define VOLATILE                                                                          volatile
#define PACKED                                                                            __attribute__((packed))
#define MS_ABI                                                                            __attribute__((ms_abi))
#define SYSV_ABI                                                                          __attribute__((sysv_abi))
#define ASM                                                                               __asm__ volatile
#define NORETURN                                                                          __attribute__((noreturn))
#define ATOMIC_LOAD(Ptr, Order)                                                           __atomic_load_n((Ptr), (Order))
#define ATOMIC_STORE(Ptr, Value, Order)                                                   __atomic_store_n((Ptr), (Value), (Order))
#define ATOMIC_EXCHANGE(Ptr, Value, Order)                                                __atomic_exchange_n((Ptr), (Value), (Order))
#define ATOMIC_FETCH_ADD(Ptr, Value, Order)                                               __atomic_fetch_add((Ptr), (Value), (Order))
#define ATOMIC_FETCH_SUB(Ptr, Value, Order)                                               __atomic_fetch_sub((Ptr), (Value), (Order))
#define ATOMIC_COMPARE_EXCHANGE(Ptr, Expected, Desired, Weak, SuccessOrder, FailureOrder) __atomic_compare_exchange_n((Ptr), (Expected), (Desired), (Weak), (SuccessOrder), (FailureOrder))
#define ATOMIC_RELAXED                                                                    __ATOMIC_RELAXED
#define ATOMIC_CONSUME                                                                    __ATOMIC_CONSUME
#define ATOMIC_ACQUIRE                                                                    __ATOMIC_ACQUIRE
#define ATOMIC_RELEASE                                                                    __ATOMIC_RELEASE
#define ATOMIC_ACQ_REL                                                                    __ATOMIC_ACQ_REL
#define ATOMIC_SEQ_CST                                                                    __ATOMIC_SEQ_CST
#define STATIC_ASSERT(Condition, Message)                                                 _Static_assert(Condition, Message)

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
typedef UINT64             SIZE;
typedef INT64              SSIZE;
typedef VOID              *PVOID;
typedef CONST VOID        *PCVOID;
typedef intptr_t           INTPTR;
typedef uintptr_t          UINTPTR;
typedef CHAR8             *PCHAR8;
typedef CHAR16            *PCHAR16;
typedef UINT8             *PUINT8;
typedef UINT32            *PUINT32;
typedef VOID             **PPVOID;
typedef CHAR8             *STRING8;
typedef CHAR16            *STRING16;
typedef INT32              PID;
typedef INT32              TID;
typedef INT32              FD;
typedef UINT32             UID;
typedef UINT32             GID;
typedef CONST CHAR8       *PCCHAR8;
typedef CONST CHAR16      *PCCHAR16;
typedef CONST UINT8       *PCUINT8;
typedef CONST UINT32      *PCUINT32;
typedef INT64              OFFSET;
typedef UINT64             COUNT;
typedef UINT64             INDEX;

#define UINT128_MAX ((UINT128) 340282366920938463463374607431768211455)
#define INT128_MAX  ((INT128) 170141183460469231731687303715884105727)
#define INT128_MIN  (-INT128_MAX - 1)
#define UINTN_MAX   18446744073709551615ULL
#define INTN_MAX    9223372036854775807LL
#define INTN_MIN    (-9223372036854775807LL - 1)