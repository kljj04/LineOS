// kernel/src/debug/panic.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <debug/panic.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <render/truetype/truetype_engine.h>
#include <arch/x86_64/cpu.h>

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

STATIC UINT32 Scale(UINT32 Value, UINT32 scale)
{
    return Value * scale;
}

STATIC VOID DrawPanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 scale)
{
    FillRect(X, Y, Width, Height, PANIC_PANEL);
    FillRect(X, Y, Scale(4, scale), Height, PANIC_ACCENT);
    DrawRect(X, Y, Width, Height, 0xFFFFFF22);
}

STATIC VOID DrawMarker(UINT32 X, UINT32 Baseline, UINT32 PixelHeight, UINT32 scale, UINT32 Color)
{
    FillCircle((INT32) X + (INT32) Scale(4, scale), (INT32) Baseline - (INT32) (PixelHeight / 3), Scale(4, scale), Color);
}

STATIC VOID DrawSectionTitle(UINT32 X, UINT32 Y, UINT32 scale, CONST CHAR16 *Title)
{
    DrawMarker(X, Y, Scale(11, scale), scale, PANIC_WARNING);
    KPrint(Title, X + Scale(18, scale), Y, PANIC_WARNING, Scale(11, scale), PRETENDARD);
}

STATIC VOID DrawPanicBackground(UINT32 Width, UINT32 scale)
{
    FillScreen(PANIC_BACKGROUND);
    FillRect(0, 0, Width, Scale(76, scale), PANIC_PANEL_DARK);
    FillRect(0, Scale(76, scale), Width, Scale(2, scale), PANIC_ACCENT);
    KPrint(L"LineOS", Scale(40, scale), Scale(48, scale), PANIC_TEXT, Scale(18, scale), PRETENDARD);
    KPrint(L"fatal diagnostic screen", Width - Scale(260, scale), Scale(48, scale), PANIC_MUTED, Scale(13, scale), PRETENDARD);
}

STATIC VOID DrawPanicFooter(UINT32 Width, UINT32 Height, UINT32 scale)
{
    UINT32 FooterY = Height - Scale(54, scale);

    FillRect(0, FooterY, Width, Scale(54, scale), PANIC_PANEL_DARK);
    DrawMarker(Scale(40, scale), FooterY + Scale(32, scale), Scale(12, scale), scale, PANIC_ACCENT);
    KPrint(L"CPU halted. Check logs/debugcon.log, then reboot the virtual machine.", Scale(60, scale), FooterY + Scale(32, scale), PANIC_MUTED, Scale(12, scale), PRETENDARD);
    KPrint(L"kljj04 / LineOS", Width - Scale(200, scale), FooterY + Scale(32, scale), PANIC_MUTED, Scale(12, scale), JETBRAINS_MONO);
}

STATIC VOID DrawPanicHeader(UINT32 X, UINT32 Y, UINT32 scale, CONST CHAR16 *Title)
{
    KPrint(Title, X, Y, PANIC_TEXT, Scale(28, scale), PRETENDARD);
    KPrint(L"The kernel stopped itself before state could drift further.", X, Y + Scale(34, scale), PANIC_MUTED, Scale(13, scale), PRETENDARD);
    FillRect(X, Y + Scale(56, scale), Scale(128, scale), Scale(3, scale), PANIC_ACCENT);
    FillRect(X + Scale(138, scale), Y + Scale(56, scale), Scale(48, scale), Scale(3, scale), PANIC_WARNING);
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

STATIC VOID DrawIconFrame(UINT32 CenterX, UINT32 CenterY, UINT32 scale, UINT32 Color)
{
    FillCircle((INT32) CenterX, (INT32) CenterY, Scale(23, scale), Color);
    FillCircle((INT32) CenterX, (INT32) CenterY, Scale(19, scale), PANIC_PANEL_DARK);
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

STATIC VOID DrawWarningIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale, UINT32 FrameColor, UINT32 MarkColor)
{
    DrawIconFrame(CenterX, CenterY, scale, FrameColor);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(11, scale)), (INT32) CenterX, (INT32) (CenterY + Scale(5, scale)), Scale(4, scale), MarkColor);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(12, scale)), Scale(3, scale), MarkColor);
}

STATIC VOID DrawPageFaultIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    UINT32 X = CenterX - Scale(14, scale);
    UINT32 Y = CenterY - Scale(14, scale);
    UINT32 Thickness = Scale(2, scale);

    DrawIconFrame(CenterX, CenterY, scale, PANIC_ACCENT);
    DrawThickRect(X, Y, Scale(28, scale), Scale(28, scale), Thickness, PANIC_TEXT);
    DrawThickLine((INT32) (X + Scale(9, scale)), (INT32) Y, (INT32) (X + Scale(9, scale)), (INT32) (Y + Scale(28, scale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) (X + Scale(19, scale)), (INT32) Y, (INT32) (X + Scale(19, scale)), (INT32) (Y + Scale(28, scale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) X, (INT32) (Y + Scale(9, scale)), (INT32) (X + Scale(28, scale)), (INT32) (Y + Scale(9, scale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) X, (INT32) (Y + Scale(19, scale)), (INT32) (X + Scale(28, scale)), (INT32) (Y + Scale(19, scale)), Thickness, PANIC_DIM);
    DrawThickLine((INT32) (CenterX - Scale(10, scale)), (INT32) (CenterY + Scale(12, scale)), (INT32) (CenterX - Scale(1, scale)), (INT32) (CenterY + Scale(2, scale)), Scale(3, scale), PANIC_ACCENT);
    DrawThickLine((INT32) (CenterX - Scale(1, scale)), (INT32) (CenterY + Scale(2, scale)), (INT32) (CenterX + Scale(7, scale)), (INT32) (CenterY - Scale(5, scale)), Scale(3, scale), PANIC_ACCENT);
}

STATIC VOID DrawProtectionIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    UINT32 Top = CenterY - Scale(15, scale);
    UINT32 Bottom = CenterY + Scale(16, scale);
    UINT32 FillColor = 0x94A3B8FF;
    UINT32 EdgeColor = 0xE5E7EBFF;

    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);

    for (UINT32 Y = Top; Y <= Bottom; Y++)
    {
        UINT32 Distance = Y - Top;
        UINT32 HalfWidth;

        if (Distance < Scale(7, scale))
        {
            HalfWidth = Scale(8, scale) + Distance;
        }
        else if (Distance < Scale(20, scale))
        {
            HalfWidth = Scale(15, scale);
        }
        else
        {
            HalfWidth = Scale(15, scale) - (Distance - Scale(20, scale));
        }

        FillRect(CenterX - HalfWidth, Y, HalfWidth * 2 + 1, 1, FillColor);
    }

    DrawThickLine((INT32) CenterX, (INT32) Top, (INT32) (CenterX + Scale(15, scale)), (INT32) (Top + Scale(7, scale)), Scale(2, scale), EdgeColor);
    DrawThickLine((INT32) (CenterX + Scale(15, scale)), (INT32) (Top + Scale(7, scale)), (INT32) (CenterX + Scale(12, scale)), (INT32) (CenterY + Scale(9, scale)), Scale(2, scale), EdgeColor);
    DrawThickLine((INT32) (CenterX + Scale(12, scale)), (INT32) (CenterY + Scale(9, scale)), (INT32) CenterX, (INT32) Bottom, Scale(2, scale), EdgeColor);
    DrawThickLine((INT32) CenterX, (INT32) Bottom, (INT32) (CenterX - Scale(12, scale)), (INT32) (CenterY + Scale(9, scale)), Scale(2, scale), EdgeColor);
    DrawThickLine((INT32) (CenterX - Scale(12, scale)), (INT32) (CenterY + Scale(9, scale)), (INT32) (CenterX - Scale(15, scale)), (INT32) (Top + Scale(7, scale)), Scale(2, scale), EdgeColor);
    DrawThickLine((INT32) (CenterX - Scale(15, scale)), (INT32) (Top + Scale(7, scale)), (INT32) CenterX, (INT32) Top, Scale(2, scale), EdgeColor);
    DrawThickLine((INT32) CenterX, (INT32) (Top + Scale(4, scale)), (INT32) CenterX, (INT32) (Bottom - Scale(5, scale)), Scale(2, scale), 0x475569FF);
}

STATIC VOID DrawDivideIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_ACCENT);
    FillCircle((INT32) CenterX, (INT32) (CenterY - Scale(10, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(13, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(13, scale)), (INT32) CenterY, Scale(4, scale), PANIC_TEXT);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(10, scale)), Scale(3, scale), PANIC_TEXT);
}

STATIC VOID DrawDebugIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_SUCCESS);
    DrawThickCircle((INT32) CenterX, (INT32) CenterY, Scale(11, scale), Scale(3, scale), PANIC_TEXT);
    FillCircle((INT32) CenterX, (INT32) CenterY, Scale(3, scale), PANIC_WARNING);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(15, scale)), (INT32) CenterX, (INT32) (CenterY - Scale(8, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(8, scale)), (INT32) CenterX, (INT32) (CenterY + Scale(15, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(15, scale)), (INT32) CenterY, (INT32) (CenterX - Scale(8, scale)), (INT32) CenterY, Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(8, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(15, scale)), (INT32) CenterY, Scale(3, scale), PANIC_TEXT);
}

STATIC VOID DrawOverflowIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawCenteredIconText(L"OF", CenterX, CenterY, Scale(20, scale), PANIC_TEXT);
}

STATIC VOID DrawBoundIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX - Scale(14, scale)), (INT32) (CenterY - Scale(12, scale)), (INT32) (CenterX - Scale(14, scale)), (INT32) (CenterY + Scale(12, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(14, scale)), (INT32) (CenterY - Scale(12, scale)), (INT32) (CenterX + Scale(14, scale)), (INT32) (CenterY + Scale(12, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(7, scale)), (INT32) CenterY, Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, scale)), (INT32) CenterY, (INT32) (CenterX - Scale(3, scale)), (INT32) (CenterY - Scale(4, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, scale)), (INT32) CenterY, (INT32) (CenterX - Scale(3, scale)), (INT32) (CenterY + Scale(4, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(7, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(3, scale)), (INT32) (CenterY - Scale(4, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(7, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(3, scale)), (INT32) (CenterY + Scale(4, scale)), Scale(3, scale), PANIC_TEXT);
}

STATIC VOID DrawDeviceIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_DIM);
    FillRect(CenterX - Scale(13, scale), CenterY - Scale(9, scale), Scale(26, scale), Scale(18, scale), 0xF5F7FA44);
    DrawThickRect(CenterX - Scale(13, scale), CenterY - Scale(9, scale), Scale(26, scale), Scale(18, scale), Scale(2, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(11, scale)), (INT32) (CenterY - Scale(14, scale)), (INT32) (CenterX + Scale(11, scale)), (INT32) (CenterY + Scale(14, scale)), Scale(3, scale), PANIC_ACCENT);
}

STATIC VOID DrawDoubleFaultIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_ACCENT);
    DrawCenteredIconText(L"!!", CenterX, CenterY, Scale(28, scale), PANIC_TEXT);
}

STATIC VOID DrawOpcodeIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_ACCENT);
    DrawCenteredIconText(L"??", CenterX, CenterY, Scale(22, scale), PANIC_TEXT);
}

STATIC VOID DrawFloatingPointIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_SUCCESS);
    DrawCenteredIconText(L"f()", CenterX, CenterY, Scale(17, scale), PANIC_TEXT);
}

STATIC VOID DrawAlignmentIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX - Scale(14, scale)), (INT32) (CenterY - Scale(9, scale)), (INT32) (CenterX + Scale(6, scale)), (INT32) (CenterY - Scale(9, scale)), Scale(3, scale), PANIC_MUTED);
    DrawThickLine((INT32) (CenterX - Scale(6, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(14, scale)), (INT32) CenterY, Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(14, scale)), (INT32) (CenterY + Scale(9, scale)), (INT32) (CenterX + Scale(6, scale)), (INT32) (CenterY + Scale(9, scale)), Scale(3, scale), PANIC_MUTED);
}

STATIC VOID DrawMachineIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_ACCENT);
    FillRect(CenterX - Scale(11, scale), CenterY - Scale(11, scale), Scale(22, scale), Scale(22, scale), 0xF5F7FA33);
    DrawThickRect(CenterX - Scale(11, scale), CenterY - Scale(11, scale), Scale(22, scale), Scale(22, scale), Scale(2, scale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(7, scale)), (INT32) CenterX, (INT32) (CenterY + Scale(2, scale)), Scale(3, scale), PANIC_WARNING);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(7, scale)), Scale(2, scale), PANIC_WARNING);
}

STATIC VOID DrawVirtualizationIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_SUCCESS);
    DrawThickRect(CenterX - Scale(15, scale), CenterY - Scale(11, scale), Scale(21, scale), Scale(15, scale), Scale(2, scale), PANIC_TEXT);
    DrawThickRect(CenterX - Scale(6, scale), CenterY - Scale(3, scale), Scale(21, scale), Scale(15, scale), Scale(2, scale), PANIC_MUTED);
}

STATIC VOID DrawBreakpointIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_SUCCESS);
    DrawThickLine((INT32) (CenterX - Scale(11, scale)), (INT32) (CenterY - Scale(12, scale)), (INT32) (CenterX - Scale(2, scale)), (INT32) (CenterY - Scale(3, scale)), Scale(4, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(11, scale)), (INT32) (CenterY - Scale(12, scale)), (INT32) (CenterX + Scale(2, scale)), (INT32) (CenterY - Scale(3, scale)), Scale(4, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(11, scale)), (INT32) (CenterY + Scale(12, scale)), (INT32) (CenterX - Scale(2, scale)), (INT32) (CenterY + Scale(3, scale)), Scale(4, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(11, scale)), (INT32) (CenterY + Scale(12, scale)), (INT32) (CenterX + Scale(2, scale)), (INT32) (CenterY + Scale(3, scale)), Scale(4, scale), PANIC_TEXT);
}

STATIC VOID DrawDefaultExceptionIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawWarningIcon(CenterX, CenterY, scale, PANIC_ACCENT, PANIC_TEXT);
}

STATIC VOID DrawNMIIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX + Scale(4, scale)), (INT32) (CenterY - Scale(15, scale)), (INT32) (CenterX - Scale(7, scale)), (INT32) CenterY, Scale(5, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(7, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(5, scale)), (INT32) CenterY, Scale(5, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(5, scale)), (INT32) CenterY, (INT32) (CenterX - Scale(4, scale)), (INT32) (CenterY + Scale(15, scale)), Scale(5, scale), PANIC_TEXT);
}

STATIC VOID DrawLegacyCoprocessorIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_DIM);
    DrawCenteredIconText(L"x87", CenterX, CenterY, Scale(16, scale), PANIC_TEXT);
}

STATIC VOID DrawTSSIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawCenteredIconText(L"TSS", CenterX, CenterY, Scale(16, scale), PANIC_TEXT);
}

STATIC VOID DrawSegmentIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawThickRect(CenterX - Scale(14, scale), CenterY - Scale(11, scale), Scale(28, scale), Scale(22, scale), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(5, scale)), (INT32) (CenterY - Scale(14, scale)), (INT32) (CenterX + Scale(5, scale)), (INT32) (CenterY + Scale(14, scale)), Scale(4, scale), PANIC_ACCENT);
}

STATIC VOID DrawStackIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);

    for (UINT32 Index = 0; Index < 3; Index++)
    {
        UINT32 Y = CenterY - Scale(10, scale) + (Index * Scale(10, scale));
        DrawThickLine((INT32) (CenterX - Scale(13, scale)), (INT32) Y, (INT32) (CenterX + Scale(13, scale)), (INT32) Y, Scale(4, scale), Index == 1 ? PANIC_TEXT : PANIC_MUTED);
    }
}

STATIC VOID DrawReservedIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_DIM);
    DrawCenteredIconText(L"R", CenterX, CenterY, Scale(24, scale), PANIC_MUTED);
}

STATIC VOID DrawControlProtectionIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawThickLine((INT32) (CenterX - Scale(9, scale)), (INT32) (CenterY - Scale(3, scale)), (INT32) (CenterX - Scale(9, scale)), (INT32) (CenterY - Scale(8, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(9, scale)), (INT32) (CenterY - Scale(8, scale)), (INT32) CenterX, (INT32) (CenterY - Scale(14, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY - Scale(14, scale)), (INT32) (CenterX + Scale(9, scale)), (INT32) (CenterY - Scale(8, scale)), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(9, scale)), (INT32) (CenterY - Scale(8, scale)), (INT32) (CenterX + Scale(9, scale)), (INT32) (CenterY - Scale(3, scale)), Scale(3, scale), PANIC_TEXT);
    FillRect(CenterX - Scale(13, scale), CenterY - Scale(3, scale), Scale(26, scale), Scale(18, scale), 0x94A3B8FF);
    FillCircle((INT32) CenterX, (INT32) (CenterY + Scale(5, scale)), Scale(3, scale), PANIC_PANEL_DARK);
}

STATIC VOID DrawHypervisorIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_SUCCESS);
    DrawCenteredIconText(L"HV", CenterX, CenterY - Scale(4, scale), Scale(17, scale), PANIC_TEXT);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(4, scale)), (INT32) CenterX, (INT32) (CenterY + Scale(14, scale)), Scale(3, scale), PANIC_WARNING);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(14, scale)), (INT32) (CenterX - Scale(5, scale)), (INT32) (CenterY + Scale(9, scale)), Scale(3, scale), PANIC_WARNING);
    DrawThickLine((INT32) CenterX, (INT32) (CenterY + Scale(14, scale)), (INT32) (CenterX + Scale(5, scale)), (INT32) (CenterY + Scale(9, scale)), Scale(3, scale), PANIC_WARNING);
}

STATIC VOID DrawCommunicationIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_SUCCESS);
    FillCircle((INT32) (CenterX - Scale(10, scale)), (INT32) CenterY, Scale(6, scale), PANIC_TEXT);
    FillCircle((INT32) (CenterX + Scale(10, scale)), (INT32) CenterY, Scale(6, scale), PANIC_MUTED);
    DrawThickLine((INT32) (CenterX - Scale(5, scale)), (INT32) CenterY, (INT32) (CenterX + Scale(5, scale)), (INT32) CenterY, Scale(4, scale), PANIC_WARNING);
}

STATIC VOID DrawSecurityKeyIcon(UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    DrawIconFrame(CenterX, CenterY, scale, PANIC_WARNING);
    DrawThickCircle((INT32) (CenterX - Scale(7, scale)), (INT32) (CenterY - Scale(4, scale)), Scale(7, scale), Scale(3, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX - Scale(2, scale)), (INT32) (CenterY + Scale(1, scale)), (INT32) (CenterX + Scale(13, scale)), (INT32) (CenterY + Scale(14, scale)), Scale(4, scale), PANIC_TEXT);
    DrawThickLine((INT32) (CenterX + Scale(7, scale)), (INT32) (CenterY + Scale(8, scale)), (INT32) (CenterX + Scale(3, scale)), (INT32) (CenterY + Scale(12, scale)), Scale(3, scale), PANIC_TEXT);
}

STATIC VOID DrawExceptionIcon(UINT64 Vector, UINT32 CenterX, UINT32 CenterY, UINT32 scale)
{
    switch (Vector)
    {
    case 0:
        DrawDivideIcon(CenterX, CenterY, scale);
        break;

    case 1:
        DrawDebugIcon(CenterX, CenterY, scale);
        break;

    case 2:
        DrawNMIIcon(CenterX, CenterY, scale);
        break;

    case 3:
        DrawBreakpointIcon(CenterX, CenterY, scale);
        break;

    case 4:
        DrawOverflowIcon(CenterX, CenterY, scale);
        break;

    case 5:
        DrawBoundIcon(CenterX, CenterY, scale);
        break;

    case 7:
        DrawDeviceIcon(CenterX, CenterY, scale);
        break;

    case 8:
        DrawDoubleFaultIcon(CenterX, CenterY, scale);
        break;

    case 9:
        DrawLegacyCoprocessorIcon(CenterX, CenterY, scale);
        break;

    case 10:
        DrawTSSIcon(CenterX, CenterY, scale);
        break;

    case 11:
        DrawSegmentIcon(CenterX, CenterY, scale);
        break;

    case 12:
        DrawStackIcon(CenterX, CenterY, scale);
        break;

    case EXCEPTION_GENERAL_PROTECTION:
        DrawProtectionIcon(CenterX, CenterY, scale);
        break;

    case EXCEPTION_PAGE_FAULT:
        DrawPageFaultIcon(CenterX, CenterY, scale);
        break;

    case 15:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 31:
        DrawReservedIcon(CenterX, CenterY, scale);
        break;

    case 6:
        DrawOpcodeIcon(CenterX, CenterY, scale);
        break;

    case 16:
    case 19:
        DrawFloatingPointIcon(CenterX, CenterY, scale);
        break;

    case 17:
        DrawAlignmentIcon(CenterX, CenterY, scale);
        break;

    case 18:
        DrawMachineIcon(CenterX, CenterY, scale);
        break;

    case 20:
        DrawVirtualizationIcon(CenterX, CenterY, scale);
        break;

    case 21:
        DrawControlProtectionIcon(CenterX, CenterY, scale);
        break;

    case 28:
        DrawHypervisorIcon(CenterX, CenterY, scale);
        break;

    case 29:
        DrawCommunicationIcon(CenterX, CenterY, scale);
        break;

    case 30:
        DrawSecurityKeyIcon(CenterX, CenterY, scale);
        break;

    default:
        DrawDefaultExceptionIcon(CenterX, CenterY, scale);
        break;
    }
}

STATIC VOID DrawFlag(UINT32 X, UINT32 Y, UINT32 scale, CONST CHAR16 *Name, BOOLEAN Enabled, CONST CHAR16 *EnabledText, CONST CHAR16 *DisabledText)
{
    UINT32 Color = Enabled ? PANIC_WARNING : PANIC_DIM;

    DrawMarker(X, Y, Scale(12, scale), scale, Color);
    KPrint(Name, X + Scale(18, scale), Y, Color, Scale(12, scale), JETBRAINS_MONO);
    KPrint(Enabled ? EnabledText : DisabledText, X + Scale(82, scale), Y, Enabled ? PANIC_TEXT : PANIC_MUTED, Scale(12, scale), PRETENDARD);
}

STATIC VOID DrawExceptionSummary(UINT32 X, UINT32 Y, UINT32 Width, UINT32 scale, INTERRUPT_FRAME *frame)
{
    CONST EXCEPTION_INFO *Info;

    DrawPanel(X, Y, Width, Scale(220, scale), scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"EXCEPTION");

    if (frame == NULL)
    {
        DrawDefaultExceptionIcon(X + Scale(44, scale), Y + Scale(106, scale), scale);
        KPrint(L"(null interrupt frame)", X + Scale(82, scale), Y + Scale(112, scale), PANIC_TEXT, Scale(22, scale), PRETENDARD);
        return;
    }

    DrawExceptionIcon(frame->Vector, X + Scale(44, scale), Y + Scale(106, scale), scale);

    Info = GetExceptionInfo(frame->Vector);

    if (Info == NULL)
    {
        KPrint(L"vector=%X", X + Scale(82, scale), Y + Scale(96, scale), PANIC_TEXT, Scale(24, scale), JETBRAINS_MONO, frame->Vector);
        KPrint(L"unknown exception", X + Scale(82, scale), Y + Scale(138, scale), PANIC_MUTED, Scale(15, scale), PRETENDARD);
        return;
    }

    KPrint(Info->Mnemonic, X + Scale(82, scale), Y + Scale(96, scale), PANIC_TEXT, Scale(26, scale), JETBRAINS_MONO);
    KPrint(Info->Name, X + Scale(82, scale), Y + Scale(138, scale), PANIC_TEXT, Scale(18, scale), PRETENDARD);
    KPrint(Info->Description, X + Scale(82, scale), Y + Scale(172, scale), PANIC_MUTED, Scale(14, scale), PRETENDARD);
}

STATIC VOID DrawPageFaultDetails(UINT32 X, UINT32 Y, UINT32 Width, UINT32 scale, UINT64 ErrorCode)
{
    UINT64 FaultAddress = ReadCR2();

    DrawPanel(X, Y, Width, Scale(220, scale), scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"PAGE FAULT DETAIL");
    KPrint(L"cr2=%X", X + Scale(24, scale), Y + Scale(78, scale), PANIC_TEXT, Scale(14, scale), JETBRAINS_MONO, FaultAddress);
    KPrint(L"error=%X", X + Scale(24, scale), Y + Scale(108, scale), PANIC_MUTED, Scale(14, scale), JETBRAINS_MONO, ErrorCode);

    DrawFlag(X + Scale(24, scale), Y + Scale(142, scale), scale, L"P", (ErrorCode & 1) != 0, L"protection", L"not present");
    DrawFlag(X + Scale(24, scale), Y + Scale(172, scale), scale, L"W", (ErrorCode & 2) != 0, L"write", L"read");

    DrawFlag(X + (Width / 2), Y + Scale(142, scale), scale, L"U", (ErrorCode & 4) != 0, L"user", L"supervisor");
    DrawFlag(X + (Width / 2), Y + Scale(172, scale), scale, L"I", (ErrorCode & 16) != 0, L"instruction", L"data");
}

STATIC VOID DrawSelectorErrorDetails(UINT32 X, UINT32 Y, UINT32 Width, UINT32 scale, UINT64 ErrorCode)
{
    UINT64 SelectorIndex = ErrorCode >> 3;
    UINT64 Table = (ErrorCode >> 1) & 3;

    DrawPanel(X, Y, Width, Scale(220, scale), scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"ERROR CODE DETAIL");
    KPrint(L"raw=%X", X + Scale(24, scale), Y + Scale(84, scale), PANIC_TEXT, Scale(15, scale), JETBRAINS_MONO, ErrorCode);
    KPrint(L"selector-index=%X", X + Scale(24, scale), Y + Scale(124, scale), PANIC_TEXT, Scale(15, scale), JETBRAINS_MONO, SelectorIndex);
    KPrint(L"external=%llu  table=%llu", X + Scale(24, scale), Y + Scale(166, scale), PANIC_MUTED, Scale(15, scale), JETBRAINS_MONO, (UINT64) (ErrorCode & 1), Table);
}

STATIC VOID DrawGenericExceptionDetails(UINT32 X, UINT32 Y, UINT32 Width, UINT32 scale, INTERRUPT_FRAME *frame)
{
    DrawPanel(X, Y, Width, Scale(220, scale), scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"FAULT DETAIL");

    if (frame == NULL)
    {
        KPrint(L"no interrupt frame", X + Scale(24, scale), Y + Scale(88, scale), PANIC_TEXT, Scale(16, scale), PRETENDARD);
        return;
    }

    KPrint(L"vector=%X  error=%X", X + Scale(24, scale), Y + Scale(88, scale), PANIC_TEXT, Scale(15, scale), JETBRAINS_MONO, frame->Vector, frame->ErrorCode);
    KPrint(L"rip=%X", X + Scale(24, scale), Y + Scale(130, scale), PANIC_MUTED, Scale(15, scale), JETBRAINS_MONO, frame->RIP);
    KPrint(L"cs=%X  rflags=%X", X + Scale(24, scale), Y + Scale(170, scale), PANIC_MUTED, Scale(15, scale), JETBRAINS_MONO, frame->CS, frame->RFLAGS);
}

STATIC VOID DrawMessagePanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 scale, CONST CHAR16 *Message)
{
    DrawPanel(X, Y, Width, Scale(220, scale), scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"PANIC MESSAGE");
    DrawWrappedText(Message, X + Scale(24, scale), Y + Scale(90, scale), Width - Scale(48, scale), Scale(19, scale), Scale(30, scale), PANIC_TEXT, PRETENDARD);
}

STATIC VOID DrawRecoveryPanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 scale)
{
    DrawPanel(X, Y, Width, Height, scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"RECOVERY");
    KPrint(L"1. Read logs/debugcon.log", X + Scale(24, scale), Y + Scale(84, scale), PANIC_TEXT, Scale(14, scale), PRETENDARD);
    KPrint(L"2. Check the caller path around RIP", X + Scale(24, scale), Y + Scale(116, scale), PANIC_TEXT, Scale(14, scale), PRETENDARD);
    KPrint(L"3. Reboot after fixing the stop reason", X + Scale(24, scale), Y + Scale(148, scale), PANIC_TEXT, Scale(14, scale), PRETENDARD);
}

STATIC VOID DrawStatusPanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 scale, PANIC_REGISTER_SNAPSHOT *Registers)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();

    DrawPanel(X, Y, Width, Height, scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"SYSTEM STATUS");
    KPrint(L"graphics     virtio-gpu", X + Scale(24, scale), Y + Scale(84, scale), PANIC_TEXT, Scale(14, scale), JETBRAINS_MONO);
    KPrint(L"framebuffer  %ux%u", X + Scale(24, scale), Y + Scale(116, scale), PANIC_TEXT, Scale(14, scale), JETBRAINS_MONO, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
    KPrint(L"panic path   software halt", X + Scale(24, scale), Y + Scale(148, scale), PANIC_MUTED, Scale(14, scale), JETBRAINS_MONO);
    KPrint(L"r10=%X  r11=%X", X + Scale(24, scale), Y + Scale(180, scale), PANIC_MUTED, Scale(14, scale), JETBRAINS_MONO, Registers->R10, Registers->R11);
}

STATIC VOID DrawExceptionStatePanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 scale, INTERRUPT_FRAME *frame)
{
    DrawPanel(X, Y, Width, Height, scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(40, scale), scale, L"CPU STATE");

    if (frame == NULL)
    {
        KPrint(L"frame pointer is null", X + Scale(24, scale), Y + Scale(84, scale), PANIC_TEXT, Scale(15, scale), PRETENDARD);
        return;
    }

    KPrint(L"rsi=%X  rdi=%X", X + Scale(24, scale), Y + Scale(84, scale), PANIC_TEXT, Scale(14, scale), JETBRAINS_MONO, frame->RSI, frame->RDI);
    KPrint(L"rbp=%X  rsp=?", X + Scale(24, scale), Y + Scale(116, scale), PANIC_TEXT, Scale(14, scale), JETBRAINS_MONO, frame->RBP);
    KPrint(L"r8 =%X  r9 =%X", X + Scale(24, scale), Y + Scale(148, scale), PANIC_MUTED, Scale(14, scale), JETBRAINS_MONO, frame->R8, frame->R9);
    KPrint(L"r10=%X  r11=%X", X + Scale(24, scale), Y + Scale(180, scale), PANIC_MUTED, Scale(14, scale), JETBRAINS_MONO, frame->R10, frame->R11);
}

STATIC VOID DrawRegisterPair(UINT32 X, UINT32 Y, UINT32 scale, CONST CHAR16 *NameA, UINT64 ValueA, CONST CHAR16 *NameB, UINT64 ValueB)
{
    KPrint(NameA, X, Y, PANIC_MUTED, Scale(13, scale), JETBRAINS_MONO);
    KPrint(L"%X", X + Scale(48, scale), Y, PANIC_TEXT, Scale(13, scale), JETBRAINS_MONO, ValueA);
    KPrint(NameB, X + Scale(300, scale), Y, PANIC_MUTED, Scale(13, scale), JETBRAINS_MONO);
    KPrint(L"%X", X + Scale(348, scale), Y, PANIC_TEXT, Scale(13, scale), JETBRAINS_MONO, ValueB);
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

STATIC VOID DrawRegisterSnapshot(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 scale, PANIC_REGISTER_SNAPSHOT *Registers)
{
    DrawPanel(X, Y, Width, Height, scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(38, scale), scale, L"REGISTER SNAPSHOT");
    KPrint(L"rip=%X rflags=%X", X + Scale(24, scale), Y + Scale(74, scale), PANIC_TEXT, Scale(12, scale), JETBRAINS_MONO, Registers->RIP, Registers->RFLAGS);

    DrawRegisterPair(X + Scale(24, scale), Y + Scale(108, scale), scale, L"RAX", Registers->RAX, L"RBX", Registers->RBX);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(130, scale), scale, L"RCX", Registers->RCX, L"RDX", Registers->RDX);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(152, scale), scale, L"RSI", Registers->RSI, L"RDI", Registers->RDI);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(174, scale), scale, L"RBP", Registers->RBP, L"RSP", Registers->RSP);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(196, scale), scale, L"R8", Registers->R8, L"R9", Registers->R9);
}

STATIC VOID DrawInterruptFrame(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 scale, INTERRUPT_FRAME *frame)
{
    DrawPanel(X, Y, Width, Height, scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(36, scale), scale, L"INTERRUPT FRAME");

    if (frame == NULL)
    {
        KPrint(L"(null frame)", X + Scale(24, scale), Y + Scale(76, scale), PANIC_TEXT, Scale(18, scale), PRETENDARD);
        return;
    }

    KPrint(L"rip=%X", X + Scale(24, scale), Y + Scale(82, scale), PANIC_TEXT, Scale(14, scale), JETBRAINS_MONO, frame->RIP);
    KPrint(L"cs=%X  rflags=%X", X + Scale(24, scale), Y + Scale(112, scale), PANIC_MUTED, Scale(14, scale), JETBRAINS_MONO, frame->CS, frame->RFLAGS);
    KPrint(L"vector=0x%X  error=%X", X + Scale(24, scale), Y + Scale(144, scale), PANIC_MUTED, Scale(14, scale), JETBRAINS_MONO, frame->Vector, frame->ErrorCode);

    DrawRegisterPair(X + Scale(24, scale), Y + Scale(184, scale), scale, L"RAX", frame->RAX, L"RBX", frame->RBX);
}

STATIC VOID HaltAfterPanic(VOID)
{
    VirtIOGPUFlush();
    CLI();
    HLT();
}

VOID Panic(CONST CHAR16 *msg)
{
    VIRTIO_GPU_INFO        *GPU = VirtIOGPUGetInfo();
    PANIC_REGISTER_SNAPSHOT Registers;

    UINT32 width = GPU->DisplayInfo.Displays[0].Rect.Width;
    UINT32 height = GPU->DisplayInfo.Displays[0].Rect.Height;
    UINT32 scale = GetScale(width, height);
    UINT32 PanelX = Scale(64, scale);
    UINT32 PanelY = Scale(184, scale);
    UINT32 PanelWidth = width - Scale(128, scale);
    UINT32 ColumnGap = Scale(24, scale);
    UINT32 LeftWidth = (PanelWidth * 2) / 5;
    UINT32 RightWidth = PanelWidth - LeftWidth - ColumnGap;
    UINT32 RightX = PanelX + LeftWidth + ColumnGap;
    UINT32 BottomY = PanelY + Scale(244, scale);

    CaptureRegisters(&Registers);
    DrawPanicBackground(width, scale);
    DrawPanicHeader(PanelX, Scale(104, scale), scale, L"Kernel Panic");
    DrawMessagePanel(PanelX, PanelY, LeftWidth, scale, msg);
    DrawRecoveryPanel(PanelX, BottomY, LeftWidth, Scale(220, scale), scale);
    DrawRegisterSnapshot(RightX, PanelY, RightWidth, Scale(220, scale), scale, &Registers);
    DrawStatusPanel(RightX, BottomY, RightWidth, Scale(220, scale), scale, &Registers);
    DrawPanicFooter(width, height, scale);
    HaltAfterPanic();
}

VOID ExceptionPanic(INTERRUPT_FRAME *frame)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();

    UINT32 width = GPU->DisplayInfo.Displays[0].Rect.Width;
    UINT32 height = GPU->DisplayInfo.Displays[0].Rect.Height;
    UINT32 scale = GetScale(width, height);
    UINT32 PanelX = Scale(64, scale);
    UINT32 PanelY = Scale(184, scale);
    UINT32 PanelWidth = width - Scale(128, scale);
    UINT32 ColumnGap = Scale(24, scale);
    UINT32 LeftWidth = (PanelWidth * 2) / 5;
    UINT32 RightWidth = PanelWidth - LeftWidth - ColumnGap;
    UINT32 RightX = PanelX + LeftWidth + ColumnGap;
    UINT32 BottomY = PanelY + Scale(244, scale);

    DrawPanicBackground(width, scale);
    DrawPanicHeader(PanelX, Scale(104, scale), scale, L"CPU Exception");
    DrawExceptionSummary(PanelX, PanelY, LeftWidth, scale, frame);

    if (frame != NULL && frame->Vector == EXCEPTION_PAGE_FAULT)
    {
        DrawPageFaultDetails(PanelX, BottomY, LeftWidth, scale, frame->ErrorCode);
    }
    else if (frame != NULL && (frame->Vector == EXCEPTION_GENERAL_PROTECTION || frame->Vector == 10 || frame->Vector == 11 || frame->Vector == 12 || frame->Vector == 17 || frame->Vector == 21 || frame->Vector == 29 || frame->Vector == 30))
    {
        DrawSelectorErrorDetails(PanelX, BottomY, LeftWidth, scale, frame->ErrorCode);
    }
    else
    {
        DrawGenericExceptionDetails(PanelX, BottomY, LeftWidth, scale, frame);
    }

    DrawInterruptFrame(RightX, PanelY, RightWidth, Scale(220, scale), scale, frame);
    DrawExceptionStatePanel(RightX, BottomY, RightWidth, Scale(220, scale), scale, frame);
    DrawPanicFooter(width, height, scale);
    HaltAfterPanic();
}
