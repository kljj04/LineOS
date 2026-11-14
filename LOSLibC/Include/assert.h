// LOSLibC/Include/assert.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#ifndef LOS_ASSERT_DECLARATIONS
#define LOS_ASSERT_DECLARATIONS

#ifdef __cplusplus
extern "C"
{
#endif

    __attribute__((noreturn)) void LOSAssertFail(const char *Expression, const char *File, unsigned int Line, const char *Function);

#ifdef __cplusplus
}
#endif

#endif

#undef assert

#ifdef NDEBUG
#define assert(Expression) ((void) 0)
#else
#define assert(Expression) ((Expression) ? (void) 0 : LOSAssertFail(#Expression, __FILE__, __LINE__, __func__))
#endif

#if !defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && __STDC_VERSION__ < 202311L
#define static_assert _Static_assert
#endif
