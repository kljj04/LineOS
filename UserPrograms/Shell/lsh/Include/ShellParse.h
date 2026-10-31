// UserPrograms/Shell/lsh/Include/ShellParse.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <Shell.h>

#define SHELL_PARSE_BYTES  8192U
#define SHELL_PARSE_TOKENS 128U

typedef enum
{
    SHELL_TOKEN_WORD,
    SHELL_TOKEN_TRUNCATE,
    SHELL_TOKEN_APPEND
} SHELL_TOKEN_TYPE;

typedef struct
{
    SHELL_TOKEN_TYPE Type;
    CONST char      *Text;
} SHELL_TOKEN;

typedef struct
{
    char        Storage[SHELL_PARSE_BYTES];
    SHELL_TOKEN Tokens[SHELL_PARSE_TOKENS];
    UINTN       Count;
    UINTN       Used;
    UINTN       Start;
    BOOLEAN     Active;
} SHELL_PARSE;

BOOLEAN ShellParseLine(CONST char *Text, char **Envp, INT32 Status, SHELL_PARSE *Parse);
INT32   ShellEchoCommand(CONST char *Text, char **Envp, INT32 Status);

BOOLEAN ShellParseAssignment(CONST char *Text, char **Envp, INT32 Status, SHELL_PARSE *Parse);
