// kernel/src/debug/panic.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Debug/Panic.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Render/TrueType/Print.h>
#include <Render/TrueType/TrueTypeEngine.h>
#include <Arch/AMD64/CPU.h>
#include <Debug/Debug.h>

#define PANIC_BACKGROUND             0x0B1020
#define PANIC_PANEL                  0x141A2ECC
#define PANIC_PANEL_DARK             0x070A12AA
#define PANIC_ACCENT                 0xFF4D6DFF
#define PANIC_WARNING                0xFFD166FF
#define PANIC_TEXT                   0xF5F7FAFF
#define PANIC_MUTED                  0xAAB4C8DD
#define PANIC_DIM                    0x677089AA
#define PANIC_SUCCESS                0x43D39EFF
#define EXCEPTION_PAGE_FAULT         14
#define EXCEPTION_GENERAL_PROTECTION 13

STATIC BOOLEAN PanicActive = FALSE;

typedef struct
{
    UINT64 RAX;
    UINT64 RBX;
    UINT64 RCX;
    UINT64 RDX;
    UINT64 RSI;
    UINT64 RDI;
    UINT64 RBP;
    UINT64 RSP;
    UINT64 R8;
    UINT64 R9;
    UINT64 R10;
    UINT64 R11;
    UINT64 RIP;
    UINT64 RFLAGS;
} PANIC_REGISTER_SNAPSHOT;

typedef struct
{
    CONST CHAR16 *Name;
    CONST CHAR16 *Mnemonic;
    CONST CHAR16 *Description;
} EXCEPTION_INFO;

STATIC CONST EXCEPTION_INFO ExceptionInfos[32] = {
    {L"Divide Error", L"#DE", L"division fault"}, {L"Debug", L"#DB", L"debug trap or fault"}, {L"Non-Maskable Interrupt", L"NMI", L"external non-maskable interrupt"}, {L"Breakpoint", L"#BP", L"breakpoint trap"}, {L"Overflow", L"#OF", L"overflow trap"}, {L"Bound Range Exceeded", L"#BR", L"bound range fault"}, {L"Invalid Opcode", L"#UD", L"unknown or invalid instruction"}, {L"Device Not Available", L"#NM", L"FPU/SIMD device unavailable"}, {L"Double Fault", L"#DF", L"nested exception escalation"}, {L"Coprocessor Segment Overrun", L"#09", L"legacy coprocessor fault"}, {L"Invalid TSS", L"#TS", L"TSS selector or descriptor fault"}, {L"Segment Not Present", L"#NP", L"segment descriptor not present"}, {L"Stack-Segment Fault", L"#SS", L"stack segment fault"}, {L"General Protection Fault", L"#GP", L"protection rule violation"}, {L"Page Fault", L"#PF", L"page translation or access fault"}, {L"Reserved", L"#15", L"reserved CPU exception"}, {L"x87 Floating-Point Exception", L"#MF", L"x87 floating-point fault"}, {L"Alignment Check", L"#AC", L"unaligned memory access"}, {L"Machine Check", L"#MC", L"hardware machine check"}, {L"SIMD Floating-Point Exception", L"#XM", L"SIMD floating-point fault"}, {L"Virtualization Exception", L"#VE", L"virtualization exception"}, {L"Control Protection Exception", L"#CP", L"control-flow protection fault"}, {L"Reserved", L"#22", L"reserved CPU exception"}, {L"Reserved", L"#23", L"reserved CPU exception"}, {L"Reserved", L"#24", L"reserved CPU exception"}, {L"Reserved", L"#25", L"reserved CPU exception"}, {L"Reserved", L"#26", L"reserved CPU exception"}, {L"Reserved", L"#27", L"reserved CPU exception"}, {L"Hypervisor Injection Exception", L"#HV", L"hypervisor injection fault"}, {L"VMM Communication Exception", L"#VC", L"VMM communication fault"}, {L"Security Exception", L"#SX", L"security-sensitive event"}, {L"Reserved", L"#31", L"reserved CPU exception"},
};

STATIC UINT32 GetScale(UINT32 Width, UINT32 Height)
{
    if (Width >= 3840 && Height >= 2160)
    {
        return 3;
    }

    if (Width >= 2560 && Height >= 1440)
    {
        return 2;
    }

    return 1;
}

STATIC UINT32 Scale(UINT32 Value, UINT32 UIScale)
{
    return Value * UIScale;
}

STATIC VOID DrawPanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 UIScale)
{
    FillRect(X, Y, Width, Height, PANIC_PANEL);
    FillRect(X, Y, Scale(4, UIScale), Height, PANIC_ACCENT);
    DrawRect(X, Y, Width, Height, 0xFFFFFF22);
}

STATIC VOID DrawMarker(UINT32 X, UINT32 Baseline, UINT32 PixelHeight, UINT32 UIScale, UINT32 Color)
{
    FillCircle((INT32) X + (INT32) Scale(4, UIScale), (INT32) Baseline - (INT32) (PixelHeight / 3), Scale(4, UIScale), Color);
}

STATIC VOID DrawSectionTitle(UINT32 X, UINT32 Y, UINT32 UIScale, CONST CHAR16 *Title)
{
    DrawMarker(X, Y, Scale(11, UIScale), UIScale, PANIC_WARNING);
    KPrint(Title, X + Scale(18, UIScale), Y, PANIC_WARNING, Scale(11, UIScale), PRETENDARD);
}

STATIC VOID DrawPanicBackground(UINT32 Width, UINT32 UIScale)
{
    FillScreen(PANIC_BACKGROUND);
    FillRect(0, 0, Width, Scale(76, UIScale), PANIC_PANEL_DARK);
    FillRect(0, Scale(76, UIScale), Width, Scale(2, UIScale), PANIC_ACCENT);
    KPrint(L"LineOS", Scale(40, UIScale), Scale(48, UIScale), PANIC_TEXT, Scale(18, UIScale), PRETENDARD);
    KPrint(L"fatal diagnostic screen", Width - Scale(260, UIScale), Scale(48, UIScale), PANIC_MUTED, Scale(13, UIScale), PRETENDARD);
}

STATIC VOID DrawPanicFooter(UINT32 Width, UINT32 Height, UINT32 UIScale)
{
    UINT32 FooterY = Height - Scale(54, UIScale);

    FillRect(0, FooterY, Width, Scale(54, UIScale), PANIC_PANEL_DARK);
    DrawMarker(Scale(40, UIScale), FooterY + Scale(32, UIScale), Scale(12, UIScale), UIScale, PANIC_ACCENT);
    KPrint(L"CPU halted. Reboot the virtual machine.", Scale(60, UIScale), FooterY + Scale(32, UIScale), PANIC_MUTED, Scale(12, UIScale), PRETENDARD);
    KPrint(L"kljj04 / LineOS", Width - Scale(200, UIScale), FooterY + Scale(32, UIScale), PANIC_MUTED, Scale(12, UIScale), JETBRAINS_MONO);
}

STATIC VOID DrawPanicHeader(UINT32 X, UINT32 Y, UINT32 UIScale, CONST CHAR16 *Title)
{
    KPrint(Title, X, Y, PANIC_TEXT, Scale(28, UIScale), PRETENDARD);
    KPrint(L"The kernel stopped itself before state could drift further.", X, Y + Scale(34, UIScale), PANIC_MUTED, Scale(13, UIScale), PRETENDARD);
    FillRect(X, Y + Scale(56, UIScale), Scale(128, UIScale), Scale(3, UIScale), PANIC_ACCENT);
    FillRect(X + Scale(138, UIScale), Y + Scale(56, UIScale), Scale(48, UIScale), Scale(3, UIScale), PANIC_WARNING);
}

STATIC UINT32 DrawWrappedText(CONST CHAR16 *Text, UINT32 X, UINT32 Baseline, UINT32 MaxWidth, UINT32 PixelHeight, UINT32 LineHeight, UINT32 Color, TRUE_TYPE_FONT Font)
{
    UINT32 StartX = X;
    UINT32 CurrentWidth = 0;
    UINT32 ApproxCharWidth = PixelHeight / 2;

    if (Text == NULL)
    {
        Text = L"(null)";
    }

    if (ApproxCharWidth == 0)
    {
        ApproxCharWidth = 1;
    }

    while (*Text != 0)
    {
        CHAR16 Char = *Text++;

        if (Char == '\n')
        {
            X = StartX;
            CurrentWidth = 0;
            Baseline += LineHeight;
            continue;
        }

        if (CurrentWidth + ApproxCharWidth > MaxWidth)
        {
            X = StartX;
            CurrentWidth = 0;
            Baseline += LineHeight;
        }

        X = DrawTrueTypeCodepoint(Font, Char, X, Baseline, Color, PixelHeight);
        CurrentWidth += ApproxCharWidth;
    }

    return Baseline;
}

STATIC CONST EXCEPTION_INFO *GetExceptionInfo(UINT64 Vector)
{
    if (Vector < 32)
    {
        return &ExceptionInfos[Vector];
    }

    return NULL;
}

STATIC UINT64 ReadCR2(VOID)
{
    UINT64 Value;

    ASM("mov %%cr2, %0" : "=r"(Value));
    return Value;
}

STATIC VOID DrawIconFrame(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale, UINT32 Color)
{
    FillCircle((INT32) CenterX, (INT32) CenterY, Scale(23, UIScale), Color);
    FillCircle((INT32) CenterX, (INT32) CenterY, Scale(19, UIScale), PANIC_PANEL_DARK);
}

STATIC VOID DrawThickLine(INT32 X1, INT32 Y1, INT32 X2, INT32 Y2, UINT32 Thickness, UINT32 Color)
{
    INT32  DeltaX = X2 > X1 ? X2 - X1 : X1 - X2;
    INT32  DeltaY = Y2 > Y1 ? Y2 - Y1 : Y1 - Y2;
    INT32  StepX = X1 < X2 ? 1 : -1;
    INT32  StepY = Y1 < Y2 ? 1 : -1;
    INT32  Error = DeltaX - DeltaY;
    UINT32 Radius = Thickness / 2;

    if (Radius == 0)
    {
        Radius = 1;
    }

    while (TRUE)
    {
        FillCircle(X1, Y1, Radius, Color);

        if (X1 == X2 && Y1 == Y2)
        {
            break;
        }

        INT32 Error2 = Error * 2;

        if (Error2 > -DeltaY)
        {
            Error -= DeltaY;
            X1 += StepX;
        }

        if (Error2 < DeltaX)
        {
            Error += DeltaX;
            Y1 += StepY;
        }
    }
}

STATIC VOID DrawThickRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Thickness, UINT32 Color)
{
    DrawThickLine((INT32) X, (INT32) Y, (INT32) (X + Width), (INT32) Y, Thickness, Color);
    DrawThickLine((INT32) (X + Width), (INT32) Y, (INT32) (X + Width), (INT32) (Y + Height), Thickness, Color);
    DrawThickLine((INT32) (X + Width), (INT32) (Y + Height), (INT32) X, (INT32) (Y + Height), Thickness, Color);
    DrawThickLine((INT32) X, (INT32) (Y + Height), (INT32) X, (INT32) Y, Thickness, Color);
}

STATIC VOID DrawThickCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Thickness, UINT32 Color)
{
    UINT32 HalfThickness = Thickness / 2;
    UINT32 InnerRadius = Radius > HalfThickness ? Radius - HalfThickness : 1;
    UINT32 OuterRadius = Radius + HalfThickness;

    for (UINT32 CurrentRadius = InnerRadius; CurrentRadius <= OuterRadius; CurrentRadius++)
    {
        DrawCircle(CenterX, CenterY, CurrentRadius, Color);
    }
}

STATIC VOID DrawCenteredIconText(CONST CHAR16 *Text, UINT32 CenterX, UINT32 CenterY, UINT32 PixelHeight, UINT32 Color)
{
    INT32 Left;
    INT32 Top;
    INT32 Right;
    INT32 Bottom;
    INT32 X;
    INT32 Baseline;

    if (!MeasureTrueTypeText(JETBRAINS_MONO, Text, PixelHeight, &Left, &Top, &Right, &Bottom))
    {
        return;
    }

    X = (INT32) CenterX - ((Left + Right) / 2);
    Baseline = (INT32) CenterY - ((Top + Bottom) / 2);
    KPrint(Text, (UINT32) X, (UINT32) Baseline, Color, PixelHeight, JETBRAINS_MONO);
}

STATIC VOID DrawWarningIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale, UINT32 FrameColor, UINT32 MarkColor)
{
    DrawIconFrame(CenterX, CenterY, UIScale, FrameColor);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(11, UIScale)), (INT32) CenterX, (INT32) (CenterY + Scale(5, UIScale)), Scale(4, UIScale), MarkColor);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(12, UIScale)), Scale(3, UIScale), MarkColor);
}

STATIC VOID DrawPageFaultIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    UINT32 X = CenterX - Scale(14, UIScale);
    UINT32 Y = CenterY - Scale(14, UIScale);
    UINT32 Thickness = Scale(2, UIScale);

    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_ACCENT);
    DrawThickRect(X, Y, Scale(28, UIScale), Scale(28, UIScale), Thickness, PANIC_TEXT);
    DrawThickLine((INT32) (X + Scale(9, UIScale)), (INT32) Y, (INT32) (X + Scale(9, UIScale)), (INT32) (Y + Scale(28, UIScale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) (X + Scale(19, UIScale)), (INT32) Y, (INT32) (X + Scale(19, UIScale)), (INT32) (Y + Scale(28, UIScale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) X, (INT32) (Y + Scale(9, UIScale)), (INT32) (X + Scale(28, UIScale)), (INT32) (Y + Scale(9, UIScale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) X, (INT32) (Y + Scale(19, UIScale)), (INT32) (X + Scale(28, UIScale)), (INT32) (Y + Scale(19, UIScale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) (CenterX - Scale(10, UIScale)), (INT32) (CenterY + Scale(12, UIScale)), (INT32) (CenterX - Scale(1, UIScale)), (INT32) (CenterY + Scale(2, UIScale)), Scale(3, UIScale), PANIC_ACCENT);
    DrawThickLine((INT32) (CenterX - Scale(1, UIScale)), (INT32) (CenterY + Scale(2, UIScale)), (INT32) (CenterX + Scale(7, UIScale)), (INT32) (CenterY - Scale(5, UIScale)), Scale(3, UIScale), PANIC_ACCENT);
}

STATIC VOID DrawProtectionIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    UINT32 Top = CenterY - Scale(15, UIScale);
    UINT32 Bottom = CenterY + Scale(16, UIScale);
    UINT32 FillColor = 0x94A3B8FF;
    UINT32 EdgeColor = 0xE5E7EBFF;

    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);

    for (UINT32 Y = Top; Y <= Bottom; Y++)
    {
        UINT32 Distance = Y - Top;
        UINT32 HalfWidth;

        if (Distance < Scale(7, UIScale))
        {
            HalfWidth = Scale(8, UIScale) + Distance;
        }
        else if (Distance < Scale(20, UIScale))
        {
            HalfWidth = Scale(15, UIScale);
        }
        else
        {
            HalfWidth = Scale(15, UIScale) - (Distance - Scale(20, UIScale));
        }

        FillRect(CenterX - HalfWidth, Y, HalfWidth * 2 + 1, 1, FillColor);
    }

    DrawThickLine((INT32) CenterX, (INT32) Top, (INT32) (CenterX + Scale(15, UIScale)), (INT32) (Top + Scale(7, UIScale)), Scale(2, UIScale), EdgeColor);
    DrawThickLine((INT32) (CenterX + Scale(15, UIScale)), (INT32) (Top + Scale(7, UIScale)), (INT32) (CenterX + Scale(12, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), Scale(2, UIScale), EdgeColor);
    DrawThickLine((INT32) (CenterX + Scale(12, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), (INT32) CenterX, (INT32) Bottom, Scale(2, UIScale), EdgeColor);
    DrawThickLine((INT32) CenterX, (INT32) Bottom, (INT32) (CenterX - Scale(12, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), Scale(2, UIScale), EdgeColor);
    DrawThickLine((INT32) (CenterX - Scale(12, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), (INT32) (CenterX - Scale(15, UIScale)), (INT32) (Top + Scale(7, UIScale)), Scale(2, UIScale), EdgeColor);
    DrawThickLine((INT32) (CenterX - Scale(15, UIScale)), (INT32) (Top + Scale(7, UIScale)), (INT32) CenterX, (INT32) Top, Scale(2, UIScale), EdgeColor);
    DrawThickLine((INT32) CenterX, (INT32) (Top + Scale(4, UIScale)), (INT32) CenterX, (INT32) (Bottom - Scale(5, UIScale)), Scale(2, UIScale), 0x475569FF);
}

STATIC VOID DrawDivideIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_ACCENT);
    FillCircle((INT32) CenterX, (INT32) (CenterY - Scale(10, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(13, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(13, UIScale)), (INT32) CenterY, Scale(4, UIScale), PANIC_TEXT);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(10, UIScale)), Scale(3, UIScale), PANIC_TEXT);
}

STATIC VOID DrawDebugIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_SUCCESS);
    DrawThickCircle((INT32) CenterX, (INT32) CenterY, Scale(11, UIScale), Scale(3, UIScale), PANIC_TEXT);
    FillCircle((INT32) CenterX, (INT32) CenterY, Scale(3, UIScale), PANIC_WARNING);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(15, UIScale)), (INT32) CenterX, (INT32) (CenterY - Scale(8, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(8, UIScale)), (INT32) CenterX, (INT32) (CenterY + Scale(15, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(15, UIScale)), (INT32) CenterY, (INT32) (CenterX - Scale(8, UIScale)), (INT32) CenterY, Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(8, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(15, UIScale)), (INT32) CenterY, Scale(3, UIScale), PANIC_TEXT);
}

STATIC VOID DrawOverflowIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawCenteredIconText(L"OF", CenterX, CenterY, Scale(20, UIScale), PANIC_TEXT);
}

STATIC VOID DrawBoundIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX - Scale(14, UIScale)), (INT32) (CenterY - Scale(12, UIScale)), (INT32) (CenterX - Scale(14, UIScale)), (INT32) (CenterY + Scale(12, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(14, UIScale)), (INT32) (CenterY - Scale(12, UIScale)), (INT32) (CenterX + Scale(14, UIScale)), (INT32) (CenterY + Scale(12, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(7, UIScale)), (INT32) CenterY, Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, UIScale)), (INT32) CenterY, (INT32) (CenterX - Scale(3, UIScale)), (INT32) (CenterY - Scale(4, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, UIScale)), (INT32) CenterY, (INT32) (CenterX - Scale(3, UIScale)), (INT32) (CenterY + Scale(4, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(7, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(3, UIScale)), (INT32) (CenterY - Scale(4, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(7, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(3, UIScale)), (INT32) (CenterY + Scale(4, UIScale)), Scale(3, UIScale), PANIC_TEXT);
}

STATIC VOID DrawDeviceIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_DIM);
    FillRect(CenterX - Scale(13, UIScale), CenterY - Scale(9, UIScale), Scale(26, UIScale), Scale(18, UIScale), 0xF5F7FA44);
    DrawThickRect(CenterX - Scale(13, UIScale), CenterY - Scale(9, UIScale), Scale(26, UIScale), Scale(18, UIScale), Scale(2, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(11, UIScale)), (INT32) (CenterY - Scale(14, UIScale)), (INT32) (CenterX + Scale(11, UIScale)), (INT32) (CenterY + Scale(14, UIScale)), Scale(3, UIScale), PANIC_ACCENT);
}

STATIC VOID DrawDoubleFaultIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_ACCENT);
    DrawCenteredIconText(L"!!", CenterX, CenterY, Scale(28, UIScale), PANIC_TEXT);
}

STATIC VOID DrawOpcodeIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_ACCENT);
    DrawCenteredIconText(L"??", CenterX, CenterY, Scale(22, UIScale), PANIC_TEXT);
}

STATIC VOID DrawFloatingPointIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_SUCCESS);
    DrawCenteredIconText(L"f()", CenterX, CenterY, Scale(17, UIScale), PANIC_TEXT);
}

STATIC VOID DrawAlignmentIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX - Scale(14, UIScale)), (INT32) (CenterY - Scale(9, UIScale)), (INT32) (CenterX + Scale(6, UIScale)), (INT32) (CenterY - Scale(9, UIScale)), Scale(3, UIScale), PANIC_MUTED);
    DrawThickLine((INT32) (CenterX - Scale(6, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(14, UIScale)), (INT32) CenterY, Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(14, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), (INT32) (CenterX + Scale(6, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), Scale(3, UIScale), PANIC_MUTED);
}

STATIC VOID DrawMachineIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_ACCENT);
    FillRect(CenterX - Scale(11, UIScale), CenterY - Scale(11, UIScale), Scale(22, UIScale), Scale(22, UIScale), 0xF5F7FA33);
    DrawThickRect(CenterX - Scale(11, UIScale), CenterY - Scale(11, UIScale), Scale(22, UIScale), Scale(22, UIScale), Scale(2, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(7, UIScale)), (INT32) CenterX, (INT32) (CenterY + Scale(2, UIScale)), Scale(3, UIScale), PANIC_WARNING);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(7, UIScale)), Scale(2, UIScale), PANIC_WARNING);
}

STATIC VOID DrawVirtualizationIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_SUCCESS);
    DrawThickRect(CenterX - Scale(15, UIScale), CenterY - Scale(11, UIScale), Scale(21, UIScale), Scale(15, UIScale), Scale(2, UIScale), PANIC_TEXT);
    DrawThickRect(CenterX - Scale(6, UIScale), CenterY - Scale(3, UIScale), Scale(21, UIScale), Scale(15, UIScale), Scale(2, UIScale), PANIC_MUTED);
}

STATIC VOID DrawBreakpointIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_SUCCESS);
    DrawThickLine((INT32) (CenterX - Scale(11, UIScale)), (INT32) (CenterY - Scale(12, UIScale)), (INT32) (CenterX - Scale(2, UIScale)), (INT32) (CenterY - Scale(3, UIScale)), Scale(4, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(11, UIScale)), (INT32) (CenterY - Scale(12, UIScale)), (INT32) (CenterX + Scale(2, UIScale)), (INT32) (CenterY - Scale(3, UIScale)), Scale(4, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(11, UIScale)), (INT32) (CenterY + Scale(12, UIScale)), (INT32) (CenterX - Scale(2, UIScale)), (INT32) (CenterY + Scale(3, UIScale)), Scale(4, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(11, UIScale)), (INT32) (CenterY + Scale(12, UIScale)), (INT32) (CenterX + Scale(2, UIScale)), (INT32) (CenterY + Scale(3, UIScale)), Scale(4, UIScale), PANIC_TEXT);
}

STATIC VOID DrawDefaultExceptionIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawWarningIcon(CenterX, CenterY, UIScale, PANIC_ACCENT, PANIC_TEXT);
}

STATIC VOID DrawNMIIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX + Scale(4, UIScale)), (INT32) (CenterY - Scale(15, UIScale)), (INT32) (CenterX - Scale(7, UIScale)), (INT32) CenterY, Scale(5, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(5, UIScale)), (INT32) CenterY, Scale(5, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(5, UIScale)), (INT32) CenterY, (INT32) (CenterX - Scale(4, UIScale)), (INT32) (CenterY + Scale(15, UIScale)), Scale(5, UIScale), PANIC_TEXT);
}

STATIC VOID DrawLegacyCoprocessorIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_DIM);
    DrawCenteredIconText(L"x87", CenterX, CenterY, Scale(16, UIScale), PANIC_TEXT);
}

STATIC VOID DrawTSSIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawCenteredIconText(L"TSS", CenterX, CenterY, Scale(16, UIScale), PANIC_TEXT);
}

STATIC VOID DrawSegmentIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawThickRect(CenterX - Scale(14, UIScale), CenterY - Scale(11, UIScale), Scale(28, UIScale), Scale(22, UIScale), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(5, UIScale)), (INT32) (CenterY - Scale(14, UIScale)), (INT32) (CenterX + Scale(5, UIScale)), (INT32) (CenterY + Scale(14, UIScale)), Scale(4, UIScale), PANIC_ACCENT);
}

STATIC VOID DrawStackIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);

    for (UINT32 Index = 0; Index < 3; Index++)
    {
        UINT32 Y = CenterY - Scale(10, UIScale) + (Index * Scale(10, UIScale));
        DrawThickLine((INT32) (CenterX - Scale(13, UIScale)), (INT32) Y, (INT32) (CenterX + Scale(13, UIScale)), (INT32) Y, Scale(4, UIScale), Index == 1 ? PANIC_TEXT : PANIC_MUTED);
    }
}

STATIC VOID DrawReservedIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_DIM);
    DrawCenteredIconText(L"R", CenterX, CenterY, Scale(24, UIScale), PANIC_MUTED);
}

STATIC VOID DrawControlProtectionIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX - Scale(9, UIScale)), (INT32) (CenterY - Scale(3, UIScale)), (INT32) (CenterX - Scale(9, UIScale)), (INT32) (CenterY - Scale(8, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(9, UIScale)), (INT32) (CenterY - Scale(8, UIScale)), (INT32) CenterX, (INT32) (CenterY - Scale(14, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(14, UIScale)), (INT32) (CenterX + Scale(9, UIScale)), (INT32) (CenterY - Scale(8, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(9, UIScale)), (INT32) (CenterY - Scale(8, UIScale)), (INT32) (CenterX + Scale(9, UIScale)), (INT32) (CenterY - Scale(3, UIScale)), Scale(3, UIScale), PANIC_TEXT);
    FillRect(CenterX - Scale(13, UIScale), CenterY - Scale(3, UIScale), Scale(26, UIScale), Scale(18, UIScale), 0x94A3B8FF);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(5, UIScale)), Scale(3, UIScale), PANIC_PANEL_DARK);
}

STATIC VOID DrawHypervisorIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_SUCCESS);
    DrawCenteredIconText(L"HV", CenterX, CenterY - Scale(4, UIScale), Scale(17, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(4, UIScale)), (INT32) CenterX, (INT32) (CenterY + Scale(14, UIScale)), Scale(3, UIScale), PANIC_WARNING);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(14, UIScale)), (INT32) (CenterX - Scale(5, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), Scale(3, UIScale), PANIC_WARNING);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(14, UIScale)), (INT32) (CenterX + Scale(5, UIScale)), (INT32) (CenterY + Scale(9, UIScale)), Scale(3, UIScale), PANIC_WARNING);
}

STATIC VOID DrawCommunicationIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_SUCCESS);
    FillCircle((INT32) (CenterX - Scale(10, UIScale)), (INT32) CenterY, Scale(6, UIScale), PANIC_TEXT);
    FillCircle((INT32) (CenterX + Scale(10, UIScale)), (INT32) CenterY, Scale(6, UIScale), PANIC_MUTED);
    DrawThickLine((INT32) (CenterX - Scale(5, UIScale)), (INT32) CenterY, (INT32) (CenterX + Scale(5, UIScale)), (INT32) CenterY, Scale(4, UIScale), PANIC_WARNING);
}

STATIC VOID DrawSecurityKeyIcon(UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    DrawIconFrame(CenterX, CenterY, UIScale, PANIC_WARNING);
    DrawThickCircle((INT32) (CenterX - Scale(7, UIScale)), (INT32) (CenterY - Scale(4, UIScale)), Scale(7, UIScale), Scale(3, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(2, UIScale)), (INT32) (CenterY + Scale(1, UIScale)), (INT32) (CenterX + Scale(13, UIScale)), (INT32) (CenterY + Scale(14, UIScale)), Scale(4, UIScale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(7, UIScale)), (INT32) (CenterY + Scale(8, UIScale)), (INT32) (CenterX + Scale(3, UIScale)), (INT32) (CenterY + Scale(12, UIScale)), Scale(3, UIScale), PANIC_TEXT);
}

STATIC VOID DrawExceptionIcon(UINT64 Vector, UINT32 CenterX, UINT32 CenterY, UINT32 UIScale)
{
    switch (Vector)
    {
    case 0:
        DrawDivideIcon(CenterX, CenterY, UIScale);
        break;

    case 1:
        DrawDebugIcon(CenterX, CenterY, UIScale);
        break;

    case 2:
        DrawNMIIcon(CenterX, CenterY, UIScale);
        break;

    case 3:
        DrawBreakpointIcon(CenterX, CenterY, UIScale);
        break;

    case 4:
        DrawOverflowIcon(CenterX, CenterY, UIScale);
        break;

    case 5:
        DrawBoundIcon(CenterX, CenterY, UIScale);
        break;

    case 7:
        DrawDeviceIcon(CenterX, CenterY, UIScale);
        break;

    case 8:
        DrawDoubleFaultIcon(CenterX, CenterY, UIScale);
        break;

    case 9:
        DrawLegacyCoprocessorIcon(CenterX, CenterY, UIScale);
        break;

    case 10:
        DrawTSSIcon(CenterX, CenterY, UIScale);
        break;

    case 11:
        DrawSegmentIcon(CenterX, CenterY, UIScale);
        break;

    case 12:
        DrawStackIcon(CenterX, CenterY, UIScale);
        break;

    case EXCEPTION_GENERAL_PROTECTION:
        DrawProtectionIcon(CenterX, CenterY, UIScale);
        break;

    case EXCEPTION_PAGE_FAULT:
        DrawPageFaultIcon(CenterX, CenterY, UIScale);
        break;

    case 15:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 31:
        DrawReservedIcon(CenterX, CenterY, UIScale);
        break;

    case 6:
        DrawOpcodeIcon(CenterX, CenterY, UIScale);
        break;

    case 16:
    case 19:
        DrawFloatingPointIcon(CenterX, CenterY, UIScale);
        break;

    case 17:
        DrawAlignmentIcon(CenterX, CenterY, UIScale);
        break;

    case 18:
        DrawMachineIcon(CenterX, CenterY, UIScale);
        break;

    case 20:
        DrawVirtualizationIcon(CenterX, CenterY, UIScale);
        break;

    case 21:
        DrawControlProtectionIcon(CenterX, CenterY, UIScale);
        break;

    case 28:
        DrawHypervisorIcon(CenterX, CenterY, UIScale);
        break;

    case 29:
        DrawCommunicationIcon(CenterX, CenterY, UIScale);
        break;

    case 30:
        DrawSecurityKeyIcon(CenterX, CenterY, UIScale);
        break;

    default:
        DrawDefaultExceptionIcon(CenterX, CenterY, UIScale);
        break;
    }
}

STATIC VOID DrawFlag(UINT32 X, UINT32 Y, UINT32 UIScale, CONST CHAR16 *Name, BOOLEAN Enabled, CONST CHAR16 *EnabledText, CONST CHAR16 *DisabledText)
{
    UINT32 Color = Enabled ? PANIC_WARNING : PANIC_DIM;

    DrawMarker(X, Y, Scale(12, UIScale), UIScale, Color);
    KPrint(Name, X + Scale(18, UIScale), Y, Color, Scale(12, UIScale), JETBRAINS_MONO);
    KPrint(Enabled ? EnabledText : DisabledText, X + Scale(82, UIScale), Y, Enabled ? PANIC_TEXT : PANIC_MUTED, Scale(12, UIScale), PRETENDARD);
}

STATIC VOID DrawExceptionSummary(UINT32 X, UINT32 Y, UINT32 Width, UINT32 UIScale, INTERRUPT_FRAME *Frame)
{
    CONST EXCEPTION_INFO *Info;

    DrawPanel(X, Y, Width, Scale(220, UIScale), UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"EXCEPTION");

    if (Frame == NULL)
    {
        DrawDefaultExceptionIcon(X + Scale(44, UIScale), Y + Scale(106, UIScale), UIScale);
        KPrint(L"(null interrupt frame)", X + Scale(82, UIScale), Y + Scale(112, UIScale), PANIC_TEXT, Scale(22, UIScale), PRETENDARD);
        return;
    }

    DrawExceptionIcon(Frame->Vector, X + Scale(44, UIScale), Y + Scale(106, UIScale), UIScale);

    Info = GetExceptionInfo(Frame->Vector);

    if (Info == NULL)
    {
        KPrint(L"vector=%X", X + Scale(82, UIScale), Y + Scale(96, UIScale), PANIC_TEXT, Scale(24, UIScale), JETBRAINS_MONO, Frame->Vector);
        KPrint(L"unknown exception", X + Scale(82, UIScale), Y + Scale(138, UIScale), PANIC_MUTED, Scale(15, UIScale), PRETENDARD);
        return;
    }

    KPrint(Info->Mnemonic, X + Scale(82, UIScale), Y + Scale(96, UIScale), PANIC_TEXT, Scale(26, UIScale), JETBRAINS_MONO);
    KPrint(Info->Name, X + Scale(82, UIScale), Y + Scale(138, UIScale), PANIC_TEXT, Scale(18, UIScale), PRETENDARD);
    KPrint(Info->Description, X + Scale(82, UIScale), Y + Scale(172, UIScale), PANIC_MUTED, Scale(14, UIScale), PRETENDARD);
}

STATIC VOID DrawPageFaultDetails(UINT32 X, UINT32 Y, UINT32 Width, UINT32 UIScale, UINT64 ErrorCode)
{
    UINT64 FaultAddress = ReadCR2();

    DrawPanel(X, Y, Width, Scale(220, UIScale), UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"PAGE FAULT DETAIL");
    KPrint(L"cr2=%X", X + Scale(24, UIScale), Y + Scale(78, UIScale), PANIC_TEXT, Scale(14, UIScale), JETBRAINS_MONO, FaultAddress);
    KPrint(L"error=%X", X + Scale(24, UIScale), Y + Scale(108, UIScale), PANIC_MUTED, Scale(14, UIScale), JETBRAINS_MONO, ErrorCode);

    DrawFlag(X + Scale(24, UIScale), Y + Scale(142, UIScale), UIScale, L"P", (ErrorCode & 1) != 0, L"protection", L"not present");
    DrawFlag(X + Scale(24, UIScale), Y + Scale(172, UIScale), UIScale, L"W", (ErrorCode & 2) != 0, L"write", L"read");

    DrawFlag(X + (Width / 2), Y + Scale(142, UIScale), UIScale, L"U", (ErrorCode & 4) != 0, L"user", L"supervisor");
    DrawFlag(X + (Width / 2), Y + Scale(172, UIScale), UIScale, L"I", (ErrorCode & 16) != 0, L"instruction", L"data");
}

STATIC VOID DrawSelectorErrorDetails(UINT32 X, UINT32 Y, UINT32 Width, UINT32 UIScale, UINT64 ErrorCode)
{
    UINT64 SelectorIndex = ErrorCode >> 3;
    UINT64 Table = (ErrorCode >> 1) & 3;

    DrawPanel(X, Y, Width, Scale(220, UIScale), UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"ERROR CODE DETAIL");
    KPrint(L"raw=%X", X + Scale(24, UIScale), Y + Scale(84, UIScale), PANIC_TEXT, Scale(15, UIScale), JETBRAINS_MONO, ErrorCode);
    KPrint(L"selector-index=%X", X + Scale(24, UIScale), Y + Scale(124, UIScale), PANIC_TEXT, Scale(15, UIScale), JETBRAINS_MONO, SelectorIndex);
    KPrint(L"external=%llu  table=%llu", X + Scale(24, UIScale), Y + Scale(166, UIScale), PANIC_MUTED, Scale(15, UIScale), JETBRAINS_MONO, (UINT64) (ErrorCode & 1), Table);
}

STATIC VOID DrawGenericExceptionDetails(UINT32 X, UINT32 Y, UINT32 Width, UINT32 UIScale, INTERRUPT_FRAME *Frame)
{
    DrawPanel(X, Y, Width, Scale(220, UIScale), UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"FAULT DETAIL");

    if (Frame == NULL)
    {
        KPrint(L"no interrupt frame", X + Scale(24, UIScale), Y + Scale(88, UIScale), PANIC_TEXT, Scale(16, UIScale), PRETENDARD);
        return;
    }

    KPrint(L"vector=%X  error=%X", X + Scale(24, UIScale), Y + Scale(88, UIScale), PANIC_TEXT, Scale(15, UIScale), JETBRAINS_MONO, Frame->Vector, Frame->ErrorCode);
    KPrint(L"rip=%X", X + Scale(24, UIScale), Y + Scale(130, UIScale), PANIC_MUTED, Scale(15, UIScale), JETBRAINS_MONO, Frame->RIP);
    KPrint(L"cs=%X  rflags=%X", X + Scale(24, UIScale), Y + Scale(170, UIScale), PANIC_MUTED, Scale(15, UIScale), JETBRAINS_MONO, Frame->CS, Frame->RFLAGS);
}

STATIC VOID DrawMessagePanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 UIScale, CONST CHAR16 *Message)
{
    DrawPanel(X, Y, Width, Scale(220, UIScale), UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"PANIC MESSAGE");
    DrawWrappedText(Message, X + Scale(24, UIScale), Y + Scale(90, UIScale), Width - Scale(48, UIScale), Scale(19, UIScale), Scale(30, UIScale), PANIC_TEXT, PRETENDARD);
}

STATIC VOID DrawRecoveryPanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 UIScale)
{
    DrawPanel(X, Y, Width, Height, UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"RECOVERY");
    KPrint(L"1. Review the panic screen", X + Scale(24, UIScale), Y + Scale(84, UIScale), PANIC_TEXT, Scale(14, UIScale), PRETENDARD);
    KPrint(L"2. Check the caller path around RIP", X + Scale(24, UIScale), Y + Scale(116, UIScale), PANIC_TEXT, Scale(14, UIScale), PRETENDARD);
    KPrint(L"3. Reboot after fixing the stop reason", X + Scale(24, UIScale), Y + Scale(148, UIScale), PANIC_TEXT, Scale(14, UIScale), PRETENDARD);
}

STATIC VOID DrawStatusPanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 UIScale, PANIC_REGISTER_SNAPSHOT *Registers)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();

    DrawPanel(X, Y, Width, Height, UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"SYSTEM STATUS");
    KPrint(L"graphics     virtio-gpu", X + Scale(24, UIScale), Y + Scale(84, UIScale), PANIC_TEXT, Scale(14, UIScale), JETBRAINS_MONO);
    KPrint(L"framebuffer  %ux%u", X + Scale(24, UIScale), Y + Scale(116, UIScale), PANIC_TEXT, Scale(14, UIScale), JETBRAINS_MONO, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
    KPrint(L"panic path   software halt", X + Scale(24, UIScale), Y + Scale(148, UIScale), PANIC_MUTED, Scale(14, UIScale), JETBRAINS_MONO);
    KPrint(L"r10=%X  r11=%X", X + Scale(24, UIScale), Y + Scale(180, UIScale), PANIC_MUTED, Scale(14, UIScale), JETBRAINS_MONO, Registers->R10, Registers->R11);
}

STATIC VOID DrawExceptionStatePanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 UIScale, INTERRUPT_FRAME *Frame)
{
    DrawPanel(X, Y, Width, Height, UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(40, UIScale), UIScale, L"CPU STATE");

    if (Frame == NULL)
    {
        KPrint(L"frame pointer is null", X + Scale(24, UIScale), Y + Scale(84, UIScale), PANIC_TEXT, Scale(15, UIScale), PRETENDARD);
        return;
    }

    KPrint(L"rsi=%X  rdi=%X", X + Scale(24, UIScale), Y + Scale(84, UIScale), PANIC_TEXT, Scale(14, UIScale), JETBRAINS_MONO, Frame->RSI, Frame->RDI);
    KPrint(L"rbp=%X  rsp=?", X + Scale(24, UIScale), Y + Scale(116, UIScale), PANIC_TEXT, Scale(14, UIScale), JETBRAINS_MONO, Frame->RBP);
    KPrint(L"r8 =%X  r9 =%X", X + Scale(24, UIScale), Y + Scale(148, UIScale), PANIC_MUTED, Scale(14, UIScale), JETBRAINS_MONO, Frame->R8, Frame->R9);
    KPrint(L"r10=%X  r11=%X", X + Scale(24, UIScale), Y + Scale(180, UIScale), PANIC_MUTED, Scale(14, UIScale), JETBRAINS_MONO, Frame->R10, Frame->R11);
}

STATIC VOID DrawRegisterPair(UINT32 X, UINT32 Y, UINT32 UIScale, CONST CHAR16 *NameA, UINT64 ValueA, CONST CHAR16 *NameB, UINT64 ValueB)
{
    KPrint(NameA, X, Y, PANIC_MUTED, Scale(13, UIScale), JETBRAINS_MONO);
    KPrint(L"%X", X + Scale(48, UIScale), Y, PANIC_TEXT, Scale(13, UIScale), JETBRAINS_MONO, ValueA);
    KPrint(NameB, X + Scale(300, UIScale), Y, PANIC_MUTED, Scale(13, UIScale), JETBRAINS_MONO);
    KPrint(L"%X", X + Scale(348, UIScale), Y, PANIC_TEXT, Scale(13, UIScale), JETBRAINS_MONO, ValueB);
}

STATIC VOID CaptureRegisters(PANIC_REGISTER_SNAPSHOT *Registers)
{
    ASM("mov %%rax, %0" : "=m"(Registers->RAX));
    ASM("mov %%rbx, %0" : "=m"(Registers->RBX));
    ASM("mov %%rcx, %0" : "=m"(Registers->RCX));
    ASM("mov %%rdx, %0" : "=m"(Registers->RDX));
    ASM("mov %%rsi, %0" : "=m"(Registers->RSI));
    ASM("mov %%rdi, %0" : "=m"(Registers->RDI));
    ASM("mov %%rbp, %0" : "=m"(Registers->RBP));
    ASM("mov %%rsp, %0" : "=m"(Registers->RSP));
    ASM("mov %%r8, %0" : "=m"(Registers->R8));
    ASM("mov %%r9, %0" : "=m"(Registers->R9));
    ASM("mov %%r10, %0" : "=m"(Registers->R10));
    ASM("mov %%r11, %0" : "=m"(Registers->R11));
    ASM("lea (%%rip), %%rax; mov %%rax, %0" : "=m"(Registers->RIP) : : "rax");
    ASM("pushfq; popq %0" : "=m"(Registers->RFLAGS));
}

STATIC VOID DrawRegisterSnapshot(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 UIScale, PANIC_REGISTER_SNAPSHOT *Registers)
{
    DrawPanel(X, Y, Width, Height, UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(38, UIScale), UIScale, L"REGISTER SNAPSHOT");
    KPrint(L"rip=%X rflags=%X", X + Scale(24, UIScale), Y + Scale(74, UIScale), PANIC_TEXT, Scale(12, UIScale), JETBRAINS_MONO, Registers->RIP, Registers->RFLAGS);

    DrawRegisterPair(X + Scale(24, UIScale), Y + Scale(108, UIScale), UIScale, L"RAX", Registers->RAX, L"RBX", Registers->RBX);
    DrawRegisterPair(X + Scale(24, UIScale), Y + Scale(130, UIScale), UIScale, L"RCX", Registers->RCX, L"RDX", Registers->RDX);
    DrawRegisterPair(X + Scale(24, UIScale), Y + Scale(152, UIScale), UIScale, L"RSI", Registers->RSI, L"RDI", Registers->RDI);
    DrawRegisterPair(X + Scale(24, UIScale), Y + Scale(174, UIScale), UIScale, L"RBP", Registers->RBP, L"RSP", Registers->RSP);
    DrawRegisterPair(X + Scale(24, UIScale), Y + Scale(196, UIScale), UIScale, L"R8", Registers->R8, L"R9", Registers->R9);
}

STATIC VOID DrawInterruptFrame(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 UIScale, INTERRUPT_FRAME *Frame)
{
    DrawPanel(X, Y, Width, Height, UIScale);
    DrawSectionTitle(X + Scale(24, UIScale), Y + Scale(36, UIScale), UIScale, L"INTERRUPT FRAME");

    if (Frame == NULL)
    {
        KPrint(L"(null frame)", X + Scale(24, UIScale), Y + Scale(76, UIScale), PANIC_TEXT, Scale(18, UIScale), PRETENDARD);
        return;
    }

    KPrint(L"rip=%X", X + Scale(24, UIScale), Y + Scale(82, UIScale), PANIC_TEXT, Scale(14, UIScale), JETBRAINS_MONO, Frame->RIP);
    KPrint(L"cs=%X  rflags=%X", X + Scale(24, UIScale), Y + Scale(112, UIScale), PANIC_MUTED, Scale(14, UIScale), JETBRAINS_MONO, Frame->CS, Frame->RFLAGS);
    KPrint(L"vector=0x%X  error=%X", X + Scale(24, UIScale), Y + Scale(144, UIScale), PANIC_MUTED, Scale(14, UIScale), JETBRAINS_MONO, Frame->Vector, Frame->ErrorCode);

    DrawRegisterPair(X + Scale(24, UIScale), Y + Scale(184, UIScale), UIScale, L"RAX", Frame->RAX, L"RBX", Frame->RBX);
}

STATIC VOID HaltAfterPanic(VOID)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();

    if (GPU != NULL && GPU->FrameBuffer != NULL)
    {
        VirtIOGPUFlush();
    }

    CLI();
    HLT();
}

STATIC VOID HaltWithoutScreen(VOID)
{
    CLI();
    HLT();
}

VOID Panic(CONST CHAR16 *Msg)
{
    VIRTIO_GPU_INFO        *GPU;
    PANIC_REGISTER_SNAPSHOT Registers;

    CLI();
    GPU = VirtIOGPUGetInfo();

    DebugWrite("PANIC: ");
    DebugWriteWide(Msg);
    DebugWrite("\n");

    if (PanicActive)
    {
        DebugWrite("PANIC: reentered\n");
        HaltWithoutScreen();
    }

    PanicActive = TRUE;

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        HaltAfterPanic();
    }

    UINT32 Width = GPU->DisplayInfo.Displays[0].Rect.Width;
    UINT32 Height = GPU->DisplayInfo.Displays[0].Rect.Height;
    UINT32 UIScale = GetScale(Width, Height);
    UINT32 PanelX = Scale(64, UIScale);
    UINT32 PanelY = Scale(184, UIScale);
    UINT32 PanelWidth = Width - Scale(128, UIScale);
    UINT32 ColumnGap = Scale(24, UIScale);
    UINT32 LeftWidth = (PanelWidth * 2) / 5;
    UINT32 RightWidth = PanelWidth - LeftWidth - ColumnGap;
    UINT32 RightX = PanelX + LeftWidth + ColumnGap;
    UINT32 BottomY = PanelY + Scale(244, UIScale);

    CaptureRegisters(&Registers);
    DrawPanicBackground(Width, UIScale);
    DrawPanicHeader(PanelX, Scale(104, UIScale), UIScale, L"Kernel Panic");
    DrawMessagePanel(PanelX, PanelY, LeftWidth, UIScale, Msg);
    DrawRecoveryPanel(PanelX, BottomY, LeftWidth, Scale(220, UIScale), UIScale);
    DrawRegisterSnapshot(RightX, PanelY, RightWidth, Scale(220, UIScale), UIScale, &Registers);
    DrawStatusPanel(RightX, BottomY, RightWidth, Scale(220, UIScale), UIScale, &Registers);
    DrawPanicFooter(Width, Height, UIScale);
    HaltAfterPanic();
}

VOID ExceptionPanic(INTERRUPT_FRAME *Frame)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();
    GPU = VirtIOGPUGetInfo();

    DebugWrite("EXCEPTION: vector=");
    DebugWriteHex(Frame == NULL ? 0 : Frame->Vector);
    DebugWrite(" rip=");
    DebugWriteHex(Frame == NULL ? 0 : Frame->RIP);
    DebugWrite(" error=");
    DebugWriteHex(Frame == NULL ? 0 : Frame->ErrorCode);
    DebugWrite("\n");

    if (PanicActive)
    {
        DebugWrite("EXCEPTION: panic reentered\n");
        HaltWithoutScreen();
    }

    PanicActive = TRUE;

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        HaltAfterPanic();
    }

    UINT32 Width = GPU->DisplayInfo.Displays[0].Rect.Width;
    UINT32 Height = GPU->DisplayInfo.Displays[0].Rect.Height;
    UINT32 UIScale = GetScale(Width, Height);
    UINT32 PanelX = Scale(64, UIScale);
    UINT32 PanelY = Scale(184, UIScale);
    UINT32 PanelWidth = Width - Scale(128, UIScale);
    UINT32 ColumnGap = Scale(24, UIScale);
    UINT32 LeftWidth = (PanelWidth * 2) / 5;
    UINT32 RightWidth = PanelWidth - LeftWidth - ColumnGap;
    UINT32 RightX = PanelX + LeftWidth + ColumnGap;
    UINT32 BottomY = PanelY + Scale(244, UIScale);

    DrawPanicBackground(Width, UIScale);
    DrawPanicHeader(PanelX, Scale(104, UIScale), UIScale, L"CPU Exception");
    DrawExceptionSummary(PanelX, PanelY, LeftWidth, UIScale, Frame);

    if (Frame != NULL && Frame->Vector == EXCEPTION_PAGE_FAULT)
    {
        DrawPageFaultDetails(PanelX, BottomY, LeftWidth, UIScale, Frame->ErrorCode);
    }
    else if (Frame != NULL && (Frame->Vector == EXCEPTION_GENERAL_PROTECTION || Frame->Vector == 10 || Frame->Vector == 11 || Frame->Vector == 12 || Frame->Vector == 17 || Frame->Vector == 21 || Frame->Vector == 29 || Frame->Vector == 30))
    {
        DrawSelectorErrorDetails(PanelX, BottomY, LeftWidth, UIScale, Frame->ErrorCode);
    }
    else
    {
        DrawGenericExceptionDetails(PanelX, BottomY, LeftWidth, UIScale, Frame);
    }

    DrawInterruptFrame(RightX, PanelY, RightWidth, Scale(220, UIScale), UIScale, Frame);
    DrawExceptionStatePanel(RightX, BottomY, RightWidth, Scale(220, UIScale), UIScale, Frame);
    DrawPanicFooter(Width, Height, UIScale);
    HaltAfterPanic();
}
