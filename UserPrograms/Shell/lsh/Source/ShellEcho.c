// UserPrograms/Shell/lsh/Source/ShellEcho.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ShellParse.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

STATIC BOOLEAN ShellEchoWrite(INT32 FD, CONST char *Text, UINTN Length)
{
    UINTN Offset = 0;

    while (Offset < Length)
    {
        ssize_t Count = write(FD, Text + Offset, Length - Offset);

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count <= 0)
        {
            if (Count == 0)
            {
                errno = EIO;
            }

            return FALSE;
        }

        Offset += (UINTN) Count;
    }

    return TRUE;
}

INT32 ShellEchoCommand(CONST char *Text, char **Envp, INT32 Status)
{
    SHELL_PARSE Parse;

    if (!ShellParseLine(Text, Envp, Status, &Parse))
    {
        puts(ShellMessage("echo: invalid quoting, expansion, or input too long", "echo: 따옴표나 확장이 잘못되었거나 입력이 너무 깁니다"));

        return 2;
    }

    for (UINTN Index = 0; Index < Parse.Count; Index++)
    {
        if (Parse.Tokens[Index].Type != SHELL_TOKEN_WORD)
        {
            if (Index + 1 == Parse.Count || Parse.Tokens[Index + 1].Type != SHELL_TOKEN_WORD || Parse.Tokens[Index + 1].Text[0] == 0)
            {
                puts(ShellMessage("echo: missing redirection filename", "echo: 출력할 파일 이름이 없습니다"));

                return 2;
            }

            Index++;
        }
    }

    char    Output[SHELL_PARSE_BYTES * 2];
    UINTN   Length = 0;
    BOOLEAN Newline = TRUE;
    BOOLEAN First = TRUE;
    INT32   FD = 1;

    for (UINTN Index = 0; Index < Parse.Count; Index++)
    {
        CONST SHELL_TOKEN *Token = &Parse.Tokens[Index];

        if (Token->Type != SHELL_TOKEN_WORD)
        {
            INT32 Flags = O_WRONLY | O_CREAT;

            if (Token->Type == SHELL_TOKEN_APPEND)
            {
                Flags |= O_APPEND;
            }
            else
            {
                Flags |= O_TRUNC;
            }

            CONST char *Path = Parse.Tokens[++Index].Text;
            INT32       Next = open(Path, Flags, 0666);

            if (Next < 0)
            {
                INT32 Error = errno;

                if (FD != 1)
                {
                    close(FD);
                }

                ShellError("echo", Path, Error);

                return 1;
            }

            if (FD != 1 && close(FD) < 0)
            {
                INT32 Error = errno;

                close(Next);
                ShellError("echo", NULL, Error);

                return 1;
            }

            FD = Next;
            continue;
        }

        if (First && strcmp(Token->Text, "-n") == 0)
        {
            Newline = FALSE;
            continue;
        }

        if (!First)
        {
            Output[Length++] = ' ';
        }

        UINTN WordLength = strlen(Token->Text);

        memcpy(Output + Length, Token->Text, WordLength);
        Length += WordLength;
        First = FALSE;
    }

    if (Newline)
    {
        Output[Length++] = '\n';
    }

    BOOLEAN Written = ShellEchoWrite(FD, Output, Length);
    INT32   Error = Written ? 0 : errno;

    if (FD != 1 && close(FD) < 0)
    {
        if (Written)
        {
            Error = errno;
        }

        Written = FALSE;
    }

    if (!Written)
    {
        ShellError("echo", NULL, Error);

        return 1;
    }

    return 0;
}
