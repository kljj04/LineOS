// UserPrograms/Shell/lsh/Source/ShellHistory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define SHELL_HISTORY_LIMIT 128U
#define SHELL_HISTORY_LINE_SIZE 256U

STATIC char History[SHELL_HISTORY_LIMIT][SHELL_HISTORY_LINE_SIZE];
STATIC UINTN HistoryCount;
STATIC UINTN HistoryPosition;
STATIC char HistoryDraft[SHELL_HISTORY_LINE_SIZE];
STATIC char HistoryPath[4096];
STATIC char HistoryTemporary[4096];

STATIC VOID ShellHistoryStore(CONST char *Line)
{
    CONST char *Text = Line;
    while (*Text == ' ')
    {
        Text++;
    }
    if (*Text == 0)
    {
        return;
    }
    if (HistoryCount != 0 && strcmp(History[HistoryCount - 1], Line) == 0)
    {
        return;
    }
    if (HistoryCount == SHELL_HISTORY_LIMIT)
    {
        memmove(History, History + 1, (SHELL_HISTORY_LIMIT - 1) * SHELL_HISTORY_LINE_SIZE);
        HistoryCount--;
    }
    strcpy(History[HistoryCount++], Line);
}

STATIC BOOLEAN ShellHistoryWrite(INT32 FD, CONST char *Text)
{
    UINTN Length = strlen(Text);
    UINTN Position = 0;
    while (Position < Length)
    {
        ssize_t Count = write(FD, Text + Position, Length - Position);
        if (Count < 0 && errno == EINTR)
        {
            continue;
        }
        if (Count <= 0)
        {
            return FALSE;
        }
        Position += (UINTN) Count;
    }
    return TRUE;
}

STATIC VOID ShellHistorySave(VOID)
{
    if (HistoryPath[0] == 0)
    {
        return;
    }
    struct stat Info;
    if (lstat(HistoryPath, &Info) == 0)
    {
        if (!S_ISREG(Info.st_mode) || Info.st_uid != getuid())
        {
            puts("lsh: history file is not owned by this user");
            return;
        }
    }
    else if (errno != ENOENT)
    {
        puts("lsh: cannot access history file");
        return;
    }
    INT32 FD = open(HistoryTemporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (FD < 0)
    {
        puts("lsh: cannot create history temporary file");
        return;
    }
    BOOLEAN Saved = TRUE;
    for (UINTN Index = 0; Index < HistoryCount; Index++)
    {
        if (!ShellHistoryWrite(FD, History[Index]) || !ShellHistoryWrite(FD, "\n"))
        {
            Saved = FALSE;
            break;
        }
    }
    if (close(FD) != 0)
    {
        Saved = FALSE;
    }
    if (Saved && rename(HistoryTemporary, HistoryPath) == 0)
    {
        return;
    }
    remove(HistoryTemporary);
    puts("lsh: cannot save command history");
}

VOID ShellHistoryInit(char **Envp)
{
    CONST char *Home = ShellGetEnvironment(Envp, "HOME");
    if (Home == NULL || *Home == 0)
    {
        return;
    }
    UINTN Length = strlen(Home);
    if (Length + sizeof("/.lsh_history.tmp") > sizeof(HistoryPath))
    {
        return;
    }
    strcpy(HistoryPath, Home);
    strcat(HistoryPath, "/.lsh_history");
    strcpy(HistoryTemporary, HistoryPath);
    strcat(HistoryTemporary, ".tmp");

    struct stat Info;
    if (lstat(HistoryPath, &Info) < 0 || !S_ISREG(Info.st_mode) || Info.st_uid != getuid())
    {
        return;
    }
    INT32 FD = open(HistoryPath, O_RDONLY);
    if (FD < 0)
    {
        return;
    }
    char Buffer[1024];
    char Line[SHELL_HISTORY_LINE_SIZE];
    UINTN Position = 0;
    BOOLEAN Invalid = FALSE;
    for (;;)
    {
        ssize_t Count = read(FD, Buffer, sizeof(Buffer));
        if (Count < 0 && errno == EINTR)
        {
            continue;
        }
        if (Count <= 0)
        {
            break;
        }
        for (ssize_t Index = 0; Index < Count; Index++)
        {
            UINT8 Byte = (UINT8) Buffer[Index];
            if (Byte == '\n')
            {
                Line[Position] = 0;
                if (!Invalid)
                {
                    ShellHistoryStore(Line);
                }
                Position = 0;
                Invalid = FALSE;
            }
            else if (Byte < 32 || Byte == 127 || Position + 1 >= sizeof(Line))
            {
                Invalid = TRUE;
            }
            else
            {
                Line[Position++] = (char) Byte;
            }
        }
    }
    if (!Invalid && Position != 0)
    {
        Line[Position] = 0;
        ShellHistoryStore(Line);
    }
    close(FD);
    HistoryPosition = HistoryCount;
}

VOID ShellHistoryAdd(CONST char *Line)
{
    ShellHistoryStore(Line);
    HistoryPosition = HistoryCount;
    HistoryDraft[0] = 0;
    ShellHistorySave();
}

BOOLEAN ShellHistoryMove(BOOLEAN Older, char Line[256], UINTN *Length)
{
    if (Older)
    {
        if (HistoryPosition == 0)
        {
            return FALSE;
        }
        if (HistoryPosition == HistoryCount)
        {
            strcpy(HistoryDraft, Line);
        }
        HistoryPosition--;
    }
    else
    {
        if (HistoryPosition >= HistoryCount)
        {
            return FALSE;
        }
        HistoryPosition++;
    }
    CONST char *Selected = HistoryDraft;
    if (HistoryPosition < HistoryCount)
    {
        Selected = History[HistoryPosition];
    }
    strcpy(Line, Selected);
    *Length = strlen(Line);
    return TRUE;
}

VOID ShellHistoryEdited(VOID)
{
    HistoryPosition = HistoryCount;
}
