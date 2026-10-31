// UserPrograms/Shell/lsh/Include/Shell.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <IME.h>

#define SHELL_INPUT_CAPACITY  4096U
#define SHELL_RENDER_CAPACITY (SHELL_INPUT_CAPACITY + 4U)

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
    SHELL_COMMAND_ASSIGNMENT,
    SHELL_COMMAND_EXTERNAL
} SHELL_COMMAND;

typedef enum
{
    SHELL_INPUT_EMPTY,
    SHELL_INPUT_BUILTIN,
    SHELL_INPUT_EXTERNAL,
    SHELL_INPUT_PATH,
    SHELL_INPUT_ASSIGNMENT,
    SHELL_INPUT_UNKNOWN
} SHELL_INPUT_STYLE;

UINTN             ShellEncode(UINT32 Codepoint, char *Bytes);
VOID              ShellEcho(UINT32 Codepoint);
VOID              ShellAppend(char *Line, UINTN *Length, UINT32 Codepoint);
VOID              ShellPrompt(CONST char *Username);
SHELL_COMMAND     ShellGetCommand(CONST char *Line);
INT32             ShellCD(CONST char *Path, char **Envp);
INT32             ShellPWD(VOID);
CONST char       *ShellGetEnvironment(char **Envp, CONST char *Name);
BOOLEAN           ShellFindCommand(CONST char *Command, char **Envp, char Path[4096]);
VOID              ShellExecute(char *Line, char **Envp, INT32 Status);
SHELL_INPUT_STYLE ShellInputStyle(CONST char *Line, IME_STATE *IME, char **Envp);
VOID              ShellInputColors(CONST char *Line, SHELL_INPUT_STYLE Style, UINT8 Colors[SHELL_RENDER_CAPACITY]);
VOID              ShellApplyInputColor(CONST char *Line, SHELL_INPUT_STYLE Style);
VOID              ShellRenderInput(CONST char *Line, IME_STATE *IME, SHELL_INPUT_STYLE Style);

INT32 ShellType(CONST char *Arguments, char **Envp);

VOID    ShellHistoryInit(char **Envp);
VOID    ShellHistoryAdd(CONST char *Line);
BOOLEAN ShellHistoryMove(BOOLEAN Older, char Line[SHELL_INPUT_CAPACITY], UINTN *Length);
VOID    ShellHistoryEdited(VOID);

BOOLEAN ShellReadCommand(char Line[SHELL_INPUT_CAPACITY], char **Envp);

VOID        ShellLocaleRefresh(VOID);
CONST char *ShellMessage(CONST char *English, CONST char *Korean);
VOID        ShellError(CONST char *Command, CONST char *Path, INT32 Error);
VOID        ShellExitNotice(BOOLEAN EOFReached, INT32 Status);
BOOLEAN     ShellExitStatus(CONST char *Text, INT32 Previous, INT32 *Status);

VOID ShellExecutionError(CONST char *Line, char **Envp, INT32 Status, INT32 Error);
VOID ShellCommandNotFound(CONST char *Name);

BOOLEAN     ShellVariableStart(char Character);
BOOLEAN     ShellVariablePart(char Character);
UINTN       ShellAssignmentLength(CONST char *Line);
CONST char *ShellGetVariable(CONST char *Name);
INT32       ShellAssign(CONST char *Line, char **Envp, INT32 Status);

VOID ShellRenderInputRange(CONST char *Line, IME_STATE *IME, SHELL_INPUT_STYLE Style, UINTN StartOffset, UINTN EndOffset);

VOID ShellRenderInputSpan(CONST char *Text, CONST UINT8 *Colors, UINTN Start, UINTN End, BOOLEAN Prompt);

BOOLEAN ShellTextReady(VOID);
VOID    ShellTextMeasure(CONST char *Text, UINT32 *Advance, UINT32 *Extent);
