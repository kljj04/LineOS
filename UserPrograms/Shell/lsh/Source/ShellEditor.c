// UserPrograms/Shell/lsh/Source/ShellEditor.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

STATIC UINTN ShellPreviousCharacter(CONST char *Line, UINTN Cursor)
{
    if (Cursor != 0)
    {
        do
        {
            Cursor--;
        } while (Cursor != 0 && ((UINT8) Line[Cursor] & 0xC0) == 0x80);
    }
    return Cursor;
}

STATIC UINTN ShellNextCharacter(CONST char *Line, UINTN Cursor, UINTN Length)
{
    if (Cursor < Length)
    {
        do
        {
            Cursor++;
        } while (Cursor < Length && ((UINT8) Line[Cursor] & 0xC0) == 0x80);
    }
    return Cursor;
}

STATIC VOID ShellInsertCharacter(char Line[256], UINTN *Length, UINTN *Cursor, UINT32 Codepoint)
{
    if (Codepoint == 0)
    {
        return;
    }
    char Bytes[3];
    UINTN Count = ShellEncode(Codepoint, Bytes);
    if (*Length + Count >= 256)
    {
        return;
    }
    memmove(Line + *Cursor + Count, Line + *Cursor, *Length - *Cursor + 1);
    memcpy(Line + *Cursor, Bytes, Count);
    *Length += Count;
    *Cursor += Count;
}

STATIC VOID ShellEditorRender(CONST char *Line, UINTN Cursor, IME_STATE *IME, char **Envp)
{
    char Text[260];
    UINTN Length = strlen(Line);
    memcpy(Text, Line, Length + 1);
    UINT32 Preedit = IMECodepoint(IME);
    if (Preedit != 0)
    {
        char Bytes[3];
        UINTN Count = ShellEncode(Preedit, Bytes);
        memmove(Text + Cursor + Count, Text + Cursor, Length - Cursor + 1);
        memcpy(Text + Cursor, Bytes, Count);
    }
    IME_STATE Empty;
    IMEReset(&Empty);
    SHELL_INPUT_STYLE Style = ShellInputStyle(Text, &Empty, Envp);
    ShellRenderInput(Text, &Empty, Style);

    UINTN Column = 5;
    for (UINTN Index = 0; Index < Cursor; Index++)
    {
        if (((UINT8) Line[Index] & 0xC0) != 0x80)
        {
            Column++;
        }
    }
    if (Preedit != 0)
    {
        Column++;
    }
    printf("\033[%luG", (unsigned long) Column);
}

STATIC VOID ShellEditorSequence(CONST char *Sequence, char Line[256], UINTN *Length, UINTN *Cursor)
{
    switch (Sequence[0])
    {
        case 'A':
        case 'B':
        {
            if (ShellHistoryMove(Sequence[0] == 'A', Line, Length))
            {
                *Cursor = *Length;
            }
            break;
        }
        case 'C':
        {
            *Cursor = ShellNextCharacter(Line, *Cursor, *Length);
            break;
        }
        case 'D':
        {
            *Cursor = ShellPreviousCharacter(Line, *Cursor);
            break;
        }
        case 'H':
        {
            *Cursor = 0;
            break;
        }
        case 'F':
        {
            *Cursor = *Length;
            break;
        }
        case '3':
        {
            if (strcmp(Sequence, "3~") == 0 && *Cursor < *Length)
            {
                UINTN End = ShellNextCharacter(Line, *Cursor, *Length);
                memmove(Line + *Cursor, Line + End, *Length - End + 1);
                *Length -= End - *Cursor;
                ShellHistoryEdited();
            }
            break;
        }
        case '<':
        {
            if (strncmp(Sequence, "<0;", 3) != 0)
            {
                break;
            }
            UINTN Position = 3;
            UINTN Column = 0;
            while (Sequence[Position] >= '0' && Sequence[Position] <= '9')
            {
                Column = Column * 10 + Sequence[Position++] - '0';
                if (Column > 4096)
                {
                    return;
                }
            }
            if (strcmp(Sequence + Position, ";1M") != 0)
            {
                break;
            }
            UINTN Characters = Column > 5 ? Column - 5 : 0;
            *Cursor = 0;
            while (Characters != 0 && *Cursor < *Length)
            {
                *Cursor = ShellNextCharacter(Line, *Cursor, *Length);
                Characters--;
            }
            break;
        }
    }
}

BOOLEAN ShellReadCommand(char Line[256], char **Envp)
{
    STATIC BOOLEAN Korean;
    UINTN Length = 0;
    UINTN Cursor = 0;
    UINTN SequenceLength = 0;
    UINT8 Escape = 0;
    char Sequence[48];
    IME_STATE IME;
    IMEReset(&IME);
    Line[0] = 0;
    CONST char *Enable = "\033[?1000h";
    CONST char *Disable = "\033[?1000l\033[0m";
    write(1, Enable, strlen(Enable));

    for (;;)
    {
        UINT8 Byte;
        if (read(0, &Byte, 1) != 1)
        {
            write(1, Disable, strlen(Disable));
            return FALSE;
        }
        if (Byte == 27)
        {
            Escape = 1;
            continue;
        }
        if (Escape == 1)
        {
            Escape = Byte == '[' ? 2 : 0;
            SequenceLength = 0;
            continue;
        }
        if (Escape == 2)
        {
            if (SequenceLength + 1 >= sizeof(Sequence))
            {
                Escape = 0;
                continue;
            }
            Sequence[SequenceLength++] = (char) Byte;
            if (Byte >= 0x40 && Byte <= 0x7E)
            {
                Sequence[SequenceLength] = 0;
                Escape = 0;
                ShellInsertCharacter(Line, &Length, &Cursor, IMECodepoint(&IME));
                IMEReset(&IME);
                ShellEditorSequence(Sequence, Line, &Length, &Cursor);
                ShellEditorRender(Line, Cursor, &IME, Envp);
            }
            continue;
        }
        if (Byte == '\n' || Byte == '\r')
        {
            ShellInsertCharacter(Line, &Length, &Cursor, IMECodepoint(&IME));
            write(1, Disable, strlen(Disable));
            write(1, "\n", 1);
            return TRUE;
        }
        if (Byte == 14)
        {
            ShellInsertCharacter(Line, &Length, &Cursor, IMECodepoint(&IME));
            IMEReset(&IME);
            Korean = !Korean;
        }
        else if (Byte == '\b' || Byte == 127)
        {
            if (IMECodepoint(&IME) != 0)
            {
                IMEBackspace(&IME);
            }
            else if (Cursor != 0)
            {
                UINTN Previous = ShellPreviousCharacter(Line, Cursor);
                memmove(Line + Previous, Line + Cursor, Length - Cursor + 1);
                Length -= Cursor - Previous;
                Cursor = Previous;
            }
        }
        else if (Byte >= 32 && Byte < 127 && Length + 6 < 256)
        {
            IME_STATE Next = IME;
            UINT32 Committed;
            if (Korean && IMEFeed(&Next, Byte, &Committed))
            {
                ShellInsertCharacter(Line, &Length, &Cursor, Committed);
                IME = Next;
            }
            else
            {
                ShellInsertCharacter(Line, &Length, &Cursor, IMECodepoint(&IME));
                IMEReset(&IME);
                ShellInsertCharacter(Line, &Length, &Cursor, Byte);
            }
        }
        else
        {
            continue;
        }
        ShellHistoryEdited();
        ShellEditorRender(Line, Cursor, &IME, Envp);
    }
}
