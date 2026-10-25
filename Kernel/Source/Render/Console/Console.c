// Kernel/Source/Render/Console/Console.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Arch/AMD64/SpinLock.h>
#include <Render/Console/Console.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Render/TrueType/TrueTypeEngine.h>

#define CONSOLE_FONT_HEIGHT 36U
#define CONSOLE_LINE_HEIGHT 36U
#define CONSOLE_TAB_WIDTH   4U
#define CONSOLE_COLOR       0xFFFFFF
#define CONSOLE_BACKGROUND  0x000000U
#define CONSOLE_HISTORY_MAX 512U

#define CONSOLE_TEXT   0U
#define CONSOLE_ESCAPE 1U
#define CONSOLE_CSI    2U

STATIC SPIN_LOCK ConsoleLock;

STATIC UINT32 ConsoleWidth;
STATIC UINT32 ConsoleHeight;
STATIC UINT32 ConsoleRows;
STATIC UINT32 ConsoleSpaceWidth;
STATIC UINT32 ConsoleX;
STATIC UINT32 ConsoleRow;

STATIC UINT32 ConsoleState;
STATIC UINT32 ConsoleParams[2];
STATIC UINT32 ConsoleParamIndex;

STATIC UINT32 ConsoleUTF8Value;
STATIC UINT32 ConsoleUTF8Minimum;
STATIC UINT32 ConsoleUTF8Remaining;

STATIC UINT32 ConsoleHistory[CONSOLE_HISTORY_MAX];
STATIC UINT32 ConsoleHistoryCount;

STATIC BOOLEAN ConsoleReady;
STATIC BOOLEAN ConsoleDirty;

STATIC VOID ConsoleResetHistory(VOID)
{
    ConsoleHistoryCount = 0;
}

STATIC VOID ConsoleRemember(UINT32 X)
{
    if (ConsoleHistoryCount < CONSOLE_HISTORY_MAX)
    {
        ConsoleHistory[ConsoleHistoryCount++] = X;
    }
}

STATIC VOID ConsoleClearScreen(VOID)
{
    FillRect(0, 0, ConsoleWidth, ConsoleHeight, CONSOLE_BACKGROUND);

    ConsoleX = 0;
    ConsoleRow = 0;
    ConsoleResetHistory();
    ConsoleDirty = TRUE;
}

STATIC VOID ConsoleScroll(VOID)
{
    UINT32 LastY;

    LastY = (ConsoleRows - 1) * CONSOLE_LINE_HEIGHT;

    if (LastY > 0)
    {
        CopyRect(0, CONSOLE_LINE_HEIGHT, ConsoleWidth, LastY, 0, 0);
    }

    FillRect(0, LastY, ConsoleWidth, ConsoleHeight - LastY, CONSOLE_BACKGROUND);

    ConsoleRow = ConsoleRows - 1;
    ConsoleResetHistory();
    ConsoleDirty = TRUE;
}

STATIC VOID ConsoleNewLine(VOID)
{
    ConsoleX = 0;
    ConsoleRow++;
    ConsoleResetHistory();

    if (ConsoleRow >= ConsoleRows)
    {
        ConsoleScroll();
    }
}

STATIC VOID ConsoleDraw(UINT32 Codepoint)
{
    UINT32 Baseline;
    UINT32 NextX;

    if (ConsoleX >= ConsoleWidth || ConsoleX + CONSOLE_FONT_HEIGHT > ConsoleWidth)
    {
        ConsoleNewLine();
    }

    Baseline = ConsoleRow * CONSOLE_LINE_HEIGHT + CONSOLE_FONT_HEIGHT;

    ConsoleRemember(ConsoleX);

    NextX = DrawTrueTypeCodepoint(JETBRAINS_MONO, Codepoint, ConsoleX, Baseline, CONSOLE_COLOR, CONSOLE_FONT_HEIGHT);

    if (NextX > ConsoleX)
    {
        ConsoleX = NextX;
        ConsoleDirty = TRUE;
    }
}

STATIC VOID ConsoleBackspace(VOID)
{
    UINT32 PreviousX;

    if (ConsoleHistoryCount > 0)
    {
        PreviousX = ConsoleHistory[--ConsoleHistoryCount];
    }
    else
    {
        PreviousX = ConsoleX > ConsoleSpaceWidth ? ConsoleX - ConsoleSpaceWidth : 0;
    }

    if (PreviousX < ConsoleX)
    {
        FillRect(PreviousX, ConsoleRow * CONSOLE_LINE_HEIGHT, ConsoleX - PreviousX, CONSOLE_LINE_HEIGHT, CONSOLE_BACKGROUND);

        ConsoleDirty = TRUE;
    }

    ConsoleX = PreviousX;
}

STATIC VOID ConsoleTab(VOID)
{
    UINT32 TabSize;
    UINT32 NextX;

    TabSize = ConsoleSpaceWidth * CONSOLE_TAB_WIDTH;
    NextX = (ConsoleX / TabSize + 1) * TabSize;

    if (NextX >= ConsoleWidth)
    {
        ConsoleNewLine();
        return;
    }

    ConsoleRemember(ConsoleX);
    ConsoleX = NextX;
}

STATIC UINT32 ConsoleParameter(UINT32 Index, UINT32 Default)
{
    if (ConsoleParams[Index] == 0)
    {
        return Default;
    }

    return ConsoleParams[Index];
}

STATIC VOID ConsoleSetPosition(UINT32 Row, UINT32 Column)
{
    UINT32 MaxColumns;

    MaxColumns = ConsoleWidth / ConsoleSpaceWidth;

    if (Row == 0)
    {
        Row = 1;
    }

    if (Column == 0)
    {
        Column = 1;
    }

    if (Row > ConsoleRows)
    {
        Row = ConsoleRows;
    }

    if (Column > MaxColumns)
    {
        Column = MaxColumns;
    }

    ConsoleRow = Row - 1;
    ConsoleX = (Column - 1) * ConsoleSpaceWidth;

    ConsoleResetHistory();
}

STATIC VOID ConsoleEraseLine(UINT32 Mode)
{
    UINT32 Y;

    Y = ConsoleRow * CONSOLE_LINE_HEIGHT;

    switch (Mode)
    {
        case 0:
            FillRect(ConsoleX, Y, ConsoleWidth - ConsoleX, CONSOLE_LINE_HEIGHT, CONSOLE_BACKGROUND);
            break;

        case 1:
            FillRect(0, Y, ConsoleX + ConsoleSpaceWidth, CONSOLE_LINE_HEIGHT, CONSOLE_BACKGROUND);
            break;

        case 2:
            FillRect(0, Y, ConsoleWidth, CONSOLE_LINE_HEIGHT, CONSOLE_BACKGROUND);
            break;

        default:
            return;
    }

    ConsoleDirty = TRUE;
}

STATIC VOID ConsoleEraseDisplay(UINT32 Mode)
{
    UINT32 Y;

    Y = ConsoleRow * CONSOLE_LINE_HEIGHT;

    switch (Mode)
    {
        case 0:
            ConsoleEraseLine(0);

            if (Y + CONSOLE_LINE_HEIGHT < ConsoleHeight)
            {
                FillRect(0, Y + CONSOLE_LINE_HEIGHT, ConsoleWidth, ConsoleHeight - Y - CONSOLE_LINE_HEIGHT, CONSOLE_BACKGROUND);
            }

            break;

        case 1:
            if (Y > 0)
            {
                FillRect(0, 0, ConsoleWidth, Y, CONSOLE_BACKGROUND);
            }

            ConsoleEraseLine(1);
            break;

        case 2:
            ConsoleClearScreen();
            return;

        default:
            return;
    }

    ConsoleDirty = TRUE;
}

STATIC VOID ConsoleExecuteCSI(CHAR8 Command)
{
    UINT32 Amount;
    UINT32 Row;
    UINT32 Column;

    Amount = ConsoleParameter(0, 1);

    switch (Command)
    {
        case 'H':
        case 'f':
            ConsoleSetPosition(ConsoleParameter(0, 1), ConsoleParameter(1, 1));
            break;

        case 'A':
            ConsoleRow = Amount > ConsoleRow ? 0 : ConsoleRow - Amount;
            ConsoleResetHistory();
            break;

        case 'B':
            ConsoleRow = Amount >= ConsoleRows - ConsoleRow ? ConsoleRows - 1 : ConsoleRow + Amount;
            ConsoleResetHistory();
            break;

        case 'C':
            Column = ConsoleX / ConsoleSpaceWidth;
            Column = Amount >= (ConsoleWidth / ConsoleSpaceWidth - 1 - Column) ? ConsoleWidth / ConsoleSpaceWidth - 1 : Column + Amount;

            ConsoleX = Column * ConsoleSpaceWidth;
            ConsoleResetHistory();
            break;

        case 'D':
            Column = ConsoleX / ConsoleSpaceWidth;
            Column = Amount > Column ? 0 : Column - Amount;

            ConsoleX = Column * ConsoleSpaceWidth;
            ConsoleResetHistory();
            break;

        case 'J':
            ConsoleEraseDisplay(ConsoleParams[0]);
            break;

        case 'K':
            ConsoleEraseLine(ConsoleParams[0]);
            break;

        case 'G':
            Row = ConsoleRow + 1;
            ConsoleSetPosition(Row, Amount);
            break;
    }
}

STATIC VOID ConsoleProcessCodepoint(UINT32 Codepoint)
{
    UINT32 Digit;

    if (ConsoleState == CONSOLE_ESCAPE)
    {
        ConsoleState = CONSOLE_TEXT;

        if (Codepoint == '[')
        {
            ConsoleParams[0] = 0;
            ConsoleParams[1] = 0;
            ConsoleParamIndex = 0;
            ConsoleState = CONSOLE_CSI;
        }

        return;
    }

    if (ConsoleState == CONSOLE_CSI)
    {
        if (Codepoint >= '0' && Codepoint <= '9')
        {
            Digit = Codepoint - '0';

            if (ConsoleParams[ConsoleParamIndex] <= (0xFFFFFFFFU - Digit) / 10)
            {
                ConsoleParams[ConsoleParamIndex] = ConsoleParams[ConsoleParamIndex] * 10 + Digit;
            }

            return;
        }

        if (Codepoint == ';')
        {
            if (ConsoleParamIndex == 0)
            {
                ConsoleParamIndex = 1;
            }

            return;
        }

        ConsoleState = CONSOLE_TEXT;

        if (Codepoint <= 127)
        {
            ConsoleExecuteCSI((CHAR8) Codepoint);
        }

        return;
    }

    switch (Codepoint)
    {
        case 0x1B:
            ConsoleState = CONSOLE_ESCAPE;
            break;

        case '\n':
            ConsoleNewLine();
            break;

        case '\r':
            ConsoleX = 0;
            ConsoleResetHistory();
            break;

        case '\t':
            ConsoleTab();
            break;

        case '\b':
            ConsoleBackspace();
            break;

        default:
            if (Codepoint >= 32 && Codepoint != 127)
            {
                ConsoleDraw(Codepoint);
            }

            break;
    }
}

STATIC VOID ConsoleProcessByte(UINT8 Byte)
{
    UINT32 Codepoint;

    if (ConsoleUTF8Remaining == 0)
    {
        if (Byte < 0x80)
        {
            ConsoleProcessCodepoint(Byte);
            return;
        }

        if (Byte >= 0xC2 && Byte <= 0xDF)
        {
            ConsoleUTF8Value = Byte & 0x1F;
            ConsoleUTF8Minimum = 0x80;
            ConsoleUTF8Remaining = 1;
            return;
        }

        if (Byte >= 0xE0 && Byte <= 0xEF)
        {
            ConsoleUTF8Value = Byte & 0x0F;
            ConsoleUTF8Minimum = 0x800;
            ConsoleUTF8Remaining = 2;
            return;
        }

        if (Byte >= 0xF0 && Byte <= 0xF4)
        {
            ConsoleUTF8Value = Byte & 0x07;
            ConsoleUTF8Minimum = 0x10000;
            ConsoleUTF8Remaining = 3;
            return;
        }

        ConsoleProcessCodepoint(0xFFFD);
        return;
    }

    if ((Byte & 0xC0) != 0x80)
    {
        ConsoleUTF8Remaining = 0;
        ConsoleProcessCodepoint(0xFFFD);
        ConsoleProcessByte(Byte);
        return;
    }

    ConsoleUTF8Value = (ConsoleUTF8Value << 6) | (Byte & 0x3F);

    ConsoleUTF8Remaining--;

    if (ConsoleUTF8Remaining != 0)
    {
        return;
    }

    Codepoint = ConsoleUTF8Value;

    if (Codepoint < ConsoleUTF8Minimum || Codepoint > 0x10FFFF || (Codepoint >= 0xD800 && Codepoint <= 0xDFFF))
    {
        Codepoint = 0xFFFD;
    }

    ConsoleProcessCodepoint(Codepoint);
}

BOOLEAN ConsoleInit(VOID)
{
    VIRTIO_GPU_INFO *GPU;
    UINT64           Flags;

    Flags = SpinLockAcquireIRQSave(&ConsoleLock);

    if (ConsoleReady)
    {
        SpinLockReleaseIRQRestore(&ConsoleLock, Flags);
        return TRUE;
    }

    GPU = VirtIOGPUGetInfo();

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        SpinLockReleaseIRQRestore(&ConsoleLock, Flags);
        return FALSE;
    }

    ConsoleWidth = GPU->FrameBufferWidth;
    ConsoleHeight = GPU->FrameBufferHeight;
    ConsoleRows = ConsoleHeight / CONSOLE_LINE_HEIGHT;

    ConsoleSpaceWidth = DrawTrueTypeCodepoint(JETBRAINS_MONO, ' ', 0, 0, CONSOLE_COLOR, CONSOLE_FONT_HEIGHT);

    if (ConsoleRows == 0 || ConsoleSpaceWidth == 0 || ConsoleWidth < ConsoleSpaceWidth)
    {
        SpinLockReleaseIRQRestore(&ConsoleLock, Flags);
        return FALSE;
    }

    ConsoleX = 0;
    ConsoleRow = 0;
    ConsoleState = CONSOLE_TEXT;
    ConsoleUTF8Remaining = 0;
    ConsoleHistoryCount = 0;
    ConsoleDirty = FALSE;
    ConsoleReady = TRUE;

    SpinLockReleaseIRQRestore(&ConsoleLock, Flags);
    return TRUE;
}

UINT64 ConsoleWrite(CONST CHAR8 *Buffer, UINT64 Count)
{
    UINT64 Flags;
    UINT64 Index;

    if (Count == 0)
    {
        return 0;
    }

    if (Buffer == NULL || !ConsoleInit())
    {
        return 0;
    }

    Flags = SpinLockAcquireIRQSave(&ConsoleLock);

    ConsoleDirty = FALSE;

    for (Index = 0; Index < Count; Index++)
    {
        ConsoleProcessByte((UINT8) Buffer[Index]);
    }

    if (ConsoleDirty)
    {
        VirtIOGPUFlush();
    }

    SpinLockReleaseIRQRestore(&ConsoleLock, Flags);

    return Count;
}