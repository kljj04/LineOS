// UserPrograms/Shell/lsh/Source/ShellLocale.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

VOID ShellLocaleRefresh(VOID)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }
}

CONST char *ShellMessage(CONST char *English, CONST char *Korean)
{
    return strcmp(setlocale(LC_MESSAGES, NULL), "ko_KR.UTF-8") == 0 ? Korean : English;
}

STATIC VOID ShellErrorWrite(CONST char *Text)
{
    UINTN Length = strlen(Text);

    while (Length != 0)
    {
        ssize_t Count = write(2, Text, Length);

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count <= 0)
        {
            break;
        }

        Text += Count;
        Length -= (UINTN) Count;
    }
}

VOID ShellError(CONST char *Command, CONST char *Path, INT32 Error)
{
    int SavedError = errno;

    ShellErrorWrite("lsh: ");
    ShellErrorWrite(Command);
    ShellErrorWrite(": ");

    if (Path != NULL && *Path != 0)
    {
        ShellErrorWrite(Path);
        ShellErrorWrite(": ");
    }

    ShellErrorWrite(strerror(Error));
    ShellErrorWrite("\n");
    errno = SavedError;
}

VOID ShellExitNotice(BOOLEAN EOFReached, INT32 Status)
{
    if (EOFReached)
    {
        puts("\033[38;2;0;134;255m[lsh: EOF]\033[0m");
    }
    else
    {
        printf("\033[38;2;214;122;122m[lsh: exit %d]\033[0m\n", Status);
    }
}

BOOLEAN ShellExitStatus(CONST char *Text, INT32 Previous, INT32 *Status)
{
    while (*Text != 0 && *Text != ' ')
    {
        Text++;
    }

    while (*Text == ' ')
    {
        Text++;
    }

    if (*Text == 0)
    {
        *Status = Previous;

        return TRUE;
    }

    BOOLEAN Negative = *Text == '-';

    if (*Text == '-' || *Text == '+')
    {
        Text++;
    }

    if (*Text < '0' || *Text > '9')
    {
        return FALSE;
    }

    UINT32 Value = 0;

    while (*Text >= '0' && *Text <= '9')
    {
        Value = (Value * 10 + (UINT32) (*Text++ - '0')) & 255U;
    }

    while (*Text == ' ')
    {
        Text++;
    }

    if (*Text != 0)
    {
        return FALSE;
    }

    *Status = (INT32) ((Negative ? 0U - Value : Value) & 255U);

    return TRUE;
}

VOID ShellCommandNotFound(CONST char *Name)
{
    int SavedError = errno;

    ShellErrorWrite(Name);
    ShellErrorWrite(ShellMessage(": command not found\n", ": 명령을 찾을 수 없습니다\n"));
    errno = SavedError;
}
