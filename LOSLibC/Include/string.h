// LOSLibC/Include/string.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>

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
