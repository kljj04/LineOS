// UserPrograms/Shell/lsh/Source/ShellEditor.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <Terminal/Text.h>

STATIC BOOLEAN EditorPixelLayout;
STATIC UINTN   EditorViewFirst;
STATIC UINTN   EditorVisibleRows = 1;
STATIC UINTN   EditorCaretRow;
STATIC UINTN   EditorRows[SHELL_RENDER_CAPACITY];

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

STATIC VOID ShellInsertCharacter(char Line[SHELL_INPUT_CAPACITY], UINTN *Length, UINTN *Cursor, UINT32 Codepoint)
{
    if (Codepoint == 0)
    {
        return;
    }

    char  Bytes[3];
    UINTN Count = ShellEncode(Codepoint, Bytes);

    if (*Length + Count >= SHELL_INPUT_CAPACITY)
    {
        return;
    }

    memmove(Line + *Cursor + Count, Line + *Cursor, *Length - *Cursor + 1);
    memcpy(Line + *Cursor, Bytes, Count);
    *Length += Count;
    *Cursor += Count;
}

STATIC UINTN ShellEditorWidth(CONST char *Line, UINTN Start, UINTN End)
{
    UINTN Width = 0;

    while (Start < End)
    {
        if (EditorPixelLayout)
        {
            UINT32 Advance;
            UINT32 Extent;

            ShellTextMeasure(Line + Start, &Advance, &Extent);
            Width += Advance;
        }
        else
        {
            Width += (UINT8) Line[Start] < 0x80 ? 1 : 2;
        }

        Start = ShellNextCharacter(Line, Start, End);
    }

    return Width;
}

STATIC VOID ShellEditorRender(CONST char *Line, UINTN Cursor, IME_STATE *IME, char **Envp)
{
    char  Text[SHELL_RENDER_CAPACITY];
    UINTN Length = strlen(Line);

    memcpy(Text, Line, Length + 1);

    UINTN  Caret = Cursor;
    UINT32 Preedit = IMECodepoint(IME);

    if (Preedit != 0)
    {
        char  Bytes[3];
        UINTN Count = ShellEncode(Preedit, Bytes);

        memmove(Text + Cursor + Count, Text + Cursor, Length - Cursor + 1);
        memcpy(Text + Cursor, Bytes, Count);
        Length += Count;
        Caret += Count;
    }

    IME_STATE Empty;

    IMEReset(&Empty);

    SHELL_INPUT_STYLE Style = ShellInputStyle(Text, &Empty, Envp);
    UINT8             Colors[SHELL_RENDER_CAPACITY];

    ShellInputColors(Text, Style, Colors);

    struct winsize Size = {0};
    UINTN          Columns = 64;
    UINTN          ScreenRows = 20;

    if (ioctl(0, TIOCGWINSZ, &Size) == 0)
    {
        if (Size.ws_col > 8)
        {
            Columns = Size.ws_col;
        }

        if (Size.ws_row > 2)
        {
            ScreenRows = Size.ws_row;
        }
    }

    EditorPixelLayout = Size.ws_xpixel > TERMINAL_MARGIN * 2 + TERMINAL_SCROLLBAR_WIDTH && ShellTextReady();

    if (EditorPixelLayout)
    {
        Columns = Size.ws_xpixel - TERMINAL_MARGIN * 2 - TERMINAL_SCROLLBAR_WIDTH;
    }
    else
    {
        Columns -= 2;
    }

    UINTN RowCount = 1;
    UINTN Width = EditorPixelLayout ? ShellEditorWidth("┗━> ", 0, strlen("┗━> ")) : 4;
    UINTN Position = 0;

    EditorRows[0] = 0;

    while (Position < Length)
    {
        UINTN Next = ShellNextCharacter(Text, Position, Length);
        UINTN Advance = ShellEditorWidth(Text, Position, Next);

        UINTN Extent = Advance;

        if (EditorPixelLayout)
        {
            UINT32 MeasuredAdvance;
            UINT32 MeasuredExtent;

            ShellTextMeasure(Text + Position, &MeasuredAdvance, &MeasuredExtent);
            Extent = MeasuredExtent;
        }

        if (Width + Extent > Columns)
        {
            EditorRows[RowCount++] = Position;
            Width = 0;
        }

        Width += Advance;
        Position = Next;
    }

    EditorRows[RowCount] = Length;

    UINTN CaretRow = 0;

    while (CaretRow + 1 < RowCount && EditorRows[CaretRow + 1] <= Caret)
    {
        CaretRow++;
    }

    UINTN Maximum = ScreenRows - 1;
    UINTN First = EditorViewFirst;

    if (First > CaretRow)
    {
        First = CaretRow;
    }

    if (CaretRow >= First + Maximum)
    {
        First = CaretRow - Maximum + 1;
    }

    if (RowCount <= Maximum)
    {
        First = 0;
    }

    write(1, "\033[?2026h", 8);

    if (EditorCaretRow != 0)
    {
        printf("\033[%luA", (unsigned long) EditorCaretRow);
    }

    write(1, "\r\033[0m", 5);

    for (UINTN Row = 0; Row < EditorVisibleRows; Row++)
    {
        write(1, "\033[2K", 4);

        if (Row + 1 < EditorVisibleRows)
        {
            write(1, "\033[B", 3);
        }
    }

    if (EditorVisibleRows > 1)
    {
        printf("\033[%luA", (unsigned long) (EditorVisibleRows - 1));
    }

    write(1, "\r", 1);

    UINTN Visible = RowCount - First;

    if (Visible > Maximum)
    {
        Visible = Maximum;
    }

    for (UINTN Row = 0; Row < Visible; Row++)
    {
        UINTN Logical = First + Row;

        ShellRenderInputSpan(Text, Colors, EditorRows[Logical], EditorRows[Logical + 1], Logical == 0);

        if (Row + 1 < Visible)
        {
            write(1, "\n", 1);
        }
    }

    EditorViewFirst = First;
    EditorVisibleRows = Visible;
    EditorCaretRow = CaretRow - First;

    if (Visible - 1 > EditorCaretRow)
    {
        printf("\033[%luA", (unsigned long) (Visible - 1 - EditorCaretRow));
    }

    UINTN Column = CaretRow == 0 ? 5 : 1;

    for (UINTN Index = EditorRows[CaretRow]; Index < Caret; Index++)
    {
        if (((UINT8) Text[Index] & 0xC0) != 0x80)
        {
            Column++;
        }
    }

    printf("\033[%luG", (unsigned long) Column);
    write(1, "\033[?2026l", 8);
}

STATIC VOID ShellEditorSequence(CONST char *Sequence, char Line[SHELL_INPUT_CAPACITY], UINTN *Length, UINTN *Cursor)
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

            UINTN Row = EditorViewFirst + EditorCaretRow;
            UINTN Prefix = Row == 0 ? 5 : 1;
            UINTN Characters = Column > Prefix ? Column - Prefix : 0;

            *Cursor = EditorRows[Row] < *Length ? EditorRows[Row] : *Length;

            while (Characters != 0 && *Cursor < *Length)
            {
                *Cursor = ShellNextCharacter(Line, *Cursor, *Length);
                Characters--;
            }

            break;
        }
    }
}

BOOLEAN ShellReadCommand(char Line[SHELL_INPUT_CAPACITY], char **Envp)
{
    STATIC BOOLEAN Korean;
    UINTN          Length = 0;
    UINTN          Cursor = 0;
    UINTN          SequenceLength = 0;
    UINT8          Escape = 0;
    char           Sequence[48];
    IME_STATE      IME;

    IMEReset(&IME);
    Line[0] = 0;
    EditorViewFirst = 0;
    EditorVisibleRows = 1;
    EditorCaretRow = 0;
    EditorRows[0] = 0;

    CONST char *Enable = "\033[?1000h";
    CONST char *Disable = "\033[?1000l\033[0m";

    write(1, Enable, strlen(Enable));

    for (;;)
    {
        UINT8   Byte;
        ssize_t Count = read(0, &Byte, 1);

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count != 1)
        {
            INT32 Error = Count == 0 ? 0 : errno;

            write(1, Disable, strlen(Disable));
            write(1, "\n", 1);
            errno = Error;

            return FALSE;
        }

        if (Byte == 3)
        {
            IMEReset(&IME);
            ShellEditorRender(Line, Length, &IME, Envp);
            write(1, Disable, strlen(Disable));
            write(1, "^C\n", 3);
            Line[0] = 0;
            errno = EINTR;

            return FALSE;
        }

        if (Byte == 4 && Escape == 0)
        {
            if (Length == 0 && IMECodepoint(&IME) == 0)
            {
                write(1, Disable, strlen(Disable));
                write(1, "\n", 1);
                errno = 0;

                return FALSE;
            }

            continue;
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
            IMEReset(&IME);
            ShellEditorRender(Line, Length, &IME, Envp);
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
        else if (Byte >= 32 && Byte < 127 && Length + 6 < SHELL_INPUT_CAPACITY)
        {
            IME_STATE Next = IME;
            UINT32    Committed;

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
