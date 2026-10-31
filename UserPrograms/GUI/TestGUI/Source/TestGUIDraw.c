// UserPrograms/GUI/TestGUI/Source/TestGUIDraw.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TestGUI.h>
#include <stddef.h>

VOID *TestGUIAllocate(UINTN Size, VOID *Context)
{
    TEST_GUI *GUI = Context;

    if (Size > TEST_GUI_ALLOCATOR_SIZE - 15)
    {
        return NULL;
    }

    UINTN Aligned = (Size + 15) & ~(UINTN) 15;

    if (GUI->Used > TEST_GUI_ALLOCATOR_SIZE - Aligned)
    {
        return NULL;
    }

    VOID *Result = GUI->Work + GUI->Used;

    GUI->Used += Aligned;

    return Result;
}

VOID TestGUIFree(VOID *Pointer, VOID *Context)
{
    (VOID) Pointer;
    (VOID) Context;
}

BOOLEAN TestGUIContains(CONST TEST_GUI_RECT *Rect, CONST POINTER_INFO *Pointer)
{
    return Pointer->X >= Rect->X && Pointer->Y >= Rect->Y && Pointer->X - Rect->X < Rect->Width && Pointer->Y - Rect->Y < Rect->Height;
}

STATIC VOID TestGUIFill(TEST_GUI *GUI, TEST_GUI_RECT Rect, UINT32 Color)
{
    for (UINT32 Row = 0; Row < Rect.Height && Rect.Y + Row < GUI->Height; Row++)
    {
        for (UINT32 Column = 0; Column < Rect.Width && Rect.X + Column < GUI->Width; Column++)
        {
            GUI->Pixels[(UINTN) (Rect.Y + Row) * GUI->Width + Rect.X + Column] = Color;
        }
    }
}

STATIC VOID TestGUIText(TEST_GUI *GUI, CONST char *Text, UINT32 X, UINT32 Y, UINT32 Size, UINT32 Color)
{
    while (*Text != 0)
    {
        GUI->Used = 0;
        X = DrawTrueTypeCodepoint(&GUI->Font, (UINT8) *Text++, X, Y, Color, Size);
    }
}

VOID TestGUIDraw(TEST_GUI *GUI)
{
    TEST_GUI_RECT Screen = {0, 0, GUI->Width, GUI->Height};

    TestGUIFill(GUI, Screen, 0x00121620);
    TestGUIFill(GUI, GUI->Window, 0x00252B38);

    UINT32        X = GUI->Window.X;
    UINT32        Y = GUI->Window.Y;
    TEST_GUI_RECT Title = {X, Y, GUI->Window.Width, 56};

    if (GUI->Client.Endpoint == 0)
    {
        TestGUIFill(GUI, Title, 0x00343F52);
        TestGUIText(GUI, "TestGUI", X + 24, Y + 38, 30, 0x00FFFFFF);
        TestGUIFill(GUI, GUI->Close, 0x00D76666);
        TestGUIText(GUI, "X", GUI->Close.X + 14, GUI->Close.Y + 28, 25, 0x00FFFFFF);
    }

    TestGUIText(GUI, "Hello, LineOS!", X + 32, Y + 120, 38, 0x00FFFFFF);
    TestGUIText(GUI, "A userspace framebuffer demo", X + 32, Y + 165, 24, 0x00B5C4DA);
    TestGUIText(GUI, "Click the button. Close with X.", X + 32, Y + 205, 22, 0x00B5C4DA);

    UINT32 ButtonColor = 0x000086FF;

    if (GUI->Hover)
    {
        ButtonColor = 0x003B9FFF;
    }

    TestGUIFill(GUI, GUI->Button, ButtonColor);
    TestGUIText(GUI, "Click me", GUI->Button.X + 22, GUI->Button.Y + 37, 28, 0x00FFFFFF);

    char   Digits[11];
    char   Reversed[10];
    UINTN  Length = 0;
    UINT32 Value = GUI->Clicks;

    do
    {
        Reversed[Length++] = '0' + Value % 10;
        Value /= 10;
    } while (Value != 0);

    for (UINTN Index = 0; Index < Length; Index++)
    {
        Digits[Index] = Reversed[Length - Index - 1];
    }

    Digits[Length] = 0;
    TestGUIText(GUI, "Clicks:", X + 230, GUI->Button.Y + 36, 26, 0x00B5C4DA);
    TestGUIText(GUI, Digits, X + 350, GUI->Button.Y + 36, 26, 0x0068CB61);
}
