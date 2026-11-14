// LOSLibC/Include/string.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


void  *memset(void *Destination, int Value, size_t Size);
void  *memcpy(void *Destination, const void *Source, size_t Size);
void  *memmove(void *Destination, const void *Source, size_t Size);
int    memcmp(const void *Left, const void *Right, size_t Size);
size_t strlen(const char *String);
int    strcmp(const char *Left, const char *Right);
int    strncmp(const char *Left, const char *Right, size_t Size);
char  *strcpy(char *Destination, const char *Source);
char  *strncpy(char *Destination, const char *Source, size_t Size);
char  *strcat(char *Destination, const char *Source);
char  *strchr(const char *String, int Character);
char  *strrchr(const char *String, int Character);
char  *strerror(int Error);
void  *memchr(const void *Buffer, int Character, size_t Size);
char  *strstr(const char *String, const char *Needle);
size_t strnlen(const char *String, size_t Limit);
char  *strdup(const char *String);
char *strtok(char *Text, const char *Delimiters);
char *strtok_r(char *Text, const char *Delimiters, char **Next);
char *strndup(const char *Text, size_t Limit);
size_t strcspn(const char *Text, const char *Reject);
char *strpbrk(const char *Text, const char *Accept);
char *strncat(char *Destination, const char *Source, size_t Limit);

#ifdef __cplusplus
}
#endif
