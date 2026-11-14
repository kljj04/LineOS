// LOSLibC/Include/stdio.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


#define EOF (-1)

int printf(const char *Format, ...);
int puts(const char *String);
int putchar(int Character);
int rename(const char *OldPath, const char *NewPath);
int remove(const char *Path);

void perror(const char *Prefix);

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE  *fopen(const char *Path, const char *Mode);
int    fclose(FILE *Stream);
size_t fread(void *Buffer, size_t Size, size_t Count, FILE *Stream);
size_t fwrite(const void *Buffer, size_t Size, size_t Count, FILE *Stream);
int    fseek(FILE *Stream, long Offset, int Whence);
long   ftell(FILE *Stream);
int    feof(FILE *Stream);
int    ferror(FILE *Stream);
void   clearerr(FILE *Stream);
int    fflush(FILE *Stream);
int    vfprintf(FILE *Stream, const char *Format, va_list Arguments);
int vprintf(const char *Format, va_list Arguments);
int    fprintf(FILE *Stream, const char *Format, ...);
int    vsnprintf(char *Buffer, size_t Capacity, const char *Format, va_list Arguments);
int    snprintf(char *Buffer, size_t Capacity, const char *Format, ...);
int getc(FILE *Stream);
int fputc(int Character, FILE *Stream);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

FILE *open_memstream(char **Buffer, size_t *Size);
FILE *fdopen(int FD, const char *Mode);
int fputs(const char *Text, FILE *Stream);
int sprintf(char *Buffer, const char *Format, ...);
int vasprintf(char **Buffer, const char *Format, va_list Arguments);
int asprintf(char **Buffer, const char *Format, ...);

#ifdef __cplusplus
}
#endif
