// UserPrograms/Shell/lsh/Include/Shell.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <IME.h>

typedef enum
{
    SHELL_COMMAND_EMPTY,
    SHELL_COMMAND_HELP,
    SHELL_COMMAND_ECHO,
    SHELL_COMMAND_CD,
    SHELL_COMMAND_PWD,
    SHELL_COMMAND_TYPE,
    SHELL_COMMAND_EXIT,
    SHELL_COMMAND_LOGOUT,
    SHELL_COMMAND_EXTERNAL
} SHELL_COMMAND;

typedef enum
{
    SHELL_INPUT_EMPTY,
    SHELL_INPUT_BUILTIN,
    SHELL_INPUT_EXTERNAL,
    SHELL_INPUT_PATH,
    SHELL_INPUT_UNKNOWN
} SHELL_INPUT_STYLE;

UINTN ShellEncode(UINT32 Codepoint, char *Bytes);
VOID ShellEcho(UINT32 Codepoint);
VOID ShellAppend(char *Line, UINTN *Length, UINT32 Codepoint);
VOID ShellPrompt(CONST char *Username);
SHELL_COMMAND ShellGetCommand(CONST char *Line);
VOID ShellCD(CONST char *Path, char **Envp);
VOID ShellPWD(VOID);
CONST char *ShellGetEnvironment(char **Envp, CONST char *Name);
BOOLEAN ShellFindCommand(CONST char *Command, char **Envp, char Path[4096]);
VOID ShellExecute(char *Line, char **Envp);
SHELL_INPUT_STYLE ShellInputStyle(CONST char *Line, IME_STATE *IME, char **Envp);
VOID ShellInputColors(CONST char *Line, SHELL_INPUT_STYLE Style, UINT8 Colors[260]);
VOID ShellApplyInputColor(CONST char *Line, SHELL_INPUT_STYLE Style);
VOID ShellRenderInput(CONST char *Line, IME_STATE *IME, SHELL_INPUT_STYLE Style);

VOID ShellType(CONST char *Arguments, char **Envp);
