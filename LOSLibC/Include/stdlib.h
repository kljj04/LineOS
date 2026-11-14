// LOSLibC/Include/stdlib.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

__attribute__((noreturn)) void exit(int ErrorCode);
__attribute__((noreturn)) void abort(void);
extern char **environ;
char         *getenv(const char *Name);


void *malloc(size_t Size);
void *calloc(size_t Count, size_t Size);
void *realloc(void *Pointer, size_t Size);
void  free(void *Pointer);
void *bsearch(const void *Key, const void *Base, size_t Count, size_t Size, int (*Compare)(const void *, const void *));
long strtol(const char *Text, char **End, int Base);
float strtof(const char *Text, char **End);
double strtod(const char *Text, char **End);
long double strtold(const char *Text, char **End);
long long strtoll(const char *Text, char **End, int Base);
float strtof(const char *Text, char **End);
double strtod(const char *Text, char **End);
long double strtold(const char *Text, char **End);
int abs(int Value);
long labs(long Value);
long long llabs(long long Value);
unsigned long long strtoull(const char *Text, char **End, int Base);
int atoi(const char *Text);
#define RAND_MAX 2147483647
int rand(void);
void srand(unsigned int Seed);

int atexit(void (*Callback)(void));
void qsort(void *Base, size_t Count, size_t Size, int (*Compare)(const void *, const void *));

#ifdef __cplusplus
}
#endif
