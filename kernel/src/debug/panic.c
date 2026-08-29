// kernel/src/debug/panic.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <debug/panic.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <render/truetype/truetype_engine.h>
#include <arch/x86_64/cpu.h>

#define PANIC_BACKGROUND 0x0B1020
#define PANIC_PANEL      0x141A2ECC
#define PANIC_PANEL_DARK 0x070A12AA
#define PANIC_ACCENT     0xFF4D6DFF
#define PANIC_WARNING    0xFFD166FF
#define PANIC_TEXT       0xF5F7FAFF
#define PANIC_MUTED      0xAAB4C8DD
#define PANIC_DIM        0x677089AA
#define PANIC_SUCCESS    0x43D39EFF

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

STATIC VOID DrawMessagePanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 scale, CONST CHAR16 *Message)
{
    DrawPanel(X, Y, Width, Scale(142, scale), scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(38, scale), scale, L"PANIC MESSAGE");
    KPrint(Message == NULL ? L"(null)" : Message, X + Scale(24, scale), Y + Scale(86, scale), PANIC_TEXT, Scale(18, scale), PRETENDARD);
}

STATIC VOID DrawRecoveryPanel(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 scale)
{
    DrawPanel(X, Y, Width, Height, scale);
    DrawSectionTitle(X + Scale(24, scale), Y + Scale(38, scale), scale, L"NEXT STEP");
    KPrint(L"1. Read logs/debugcon.log", X + Scale(24, scale), Y + Scale(78, scale), PANIC_TEXT, Scale(12, scale), PRETENDARD);
    KPrint(L"2. Check the last interrupt or panic message", X + Scale(24, scale), Y + Scale(104, scale), PANIC_TEXT, Scale(12, scale), PRETENDARD);
    KPrint(L"3. Reboot after fixing the fault path", X + Scale(24, scale), Y + Scale(130, scale), PANIC_TEXT, Scale(12, scale), PRETENDARD);
}

STATIC VOID DrawRegisterPair(UINT32 X, UINT32 Y, UINT32 scale, CONST CHAR16 *NameA, UINT64 ValueA, CONST CHAR16 *NameB, UINT64 ValueB)
{
    KPrint(NameA, X, Y, PANIC_MUTED, Scale(11, scale), JETBRAINS_MONO);
    KPrint(L"%X", X + Scale(42, scale), Y, PANIC_TEXT, Scale(11, scale), JETBRAINS_MONO, ValueA);
    KPrint(NameB, X + Scale(260, scale), Y, PANIC_MUTED, Scale(11, scale), JETBRAINS_MONO);
    KPrint(L"%X", X + Scale(302, scale), Y, PANIC_TEXT, Scale(11, scale), JETBRAINS_MONO, ValueB);
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
    KPrint(L"rip=%X rflags=%X", X + Scale(24, scale), Y + Scale(74, scale), PANIC_TEXT, Scale(11, scale), JETBRAINS_MONO, Registers->RIP, Registers->RFLAGS);

    DrawRegisterPair(X + Scale(24, scale), Y + Scale(114, scale), scale, L"RAX", Registers->RAX, L"RBX", Registers->RBX);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(136, scale), scale, L"RCX", Registers->RCX, L"RDX", Registers->RDX);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(158, scale), scale, L"RSI", Registers->RSI, L"RDI", Registers->RDI);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(180, scale), scale, L"RBP", Registers->RBP, L"RSP", Registers->RSP);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(202, scale), scale, L"R8", Registers->R8, L"R9", Registers->R9);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(224, scale), scale, L"R10", Registers->R10, L"R11", Registers->R11);
}

STATIC VOID DrawInterruptFrame(UINT32 X, UINT32 Y, UINT32 Width, UINT32 scale, INTERRUPT_FRAME *frame)
{
    DrawPanel(X, Y, Width, Scale(250, scale), scale);
    KPrint(L"INTERRUPT FRAME", X + Scale(24, scale), Y + Scale(34, scale), PANIC_WARNING, Scale(11, scale), JETBRAINS_MONO);

    if (frame == NULL)
    {
        KPrint(L"(null frame)", X + Scale(24, scale), Y + Scale(70, scale), PANIC_TEXT, Scale(14, scale), PRETENDARD);
        return;
    }

    KPrint(L"vector=%X error=%X rip=%X rflags=%X", X + Scale(24, scale), Y + Scale(70, scale), PANIC_TEXT, Scale(12, scale), JETBRAINS_MONO, frame->Vector, frame->ErrorCode, frame->RIP, frame->RFLAGS);

    DrawRegisterPair(X + Scale(24, scale), Y + Scale(112, scale), scale, L"RAX", frame->RAX, L"RBX", frame->RBX);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(134, scale), scale, L"RCX", frame->RCX, L"RDX", frame->RDX);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(156, scale), scale, L"RSI", frame->RSI, L"RDI", frame->RDI);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(178, scale), scale, L"RBP", frame->RBP, L"RSP", 0);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(200, scale), scale, L"R8", frame->R8, L"R9", frame->R9);
    DrawRegisterPair(X + Scale(24, scale), Y + Scale(222, scale), scale, L"R10", frame->R10, L"R11", frame->R11);
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
    UINT32 LeftWidth = (PanelWidth * 3) / 5;
    UINT32 RightWidth = PanelWidth - LeftWidth - ColumnGap;
    UINT32 RightX = PanelX + LeftWidth + ColumnGap;

    CaptureRegisters(&Registers);
    DrawPanicBackground(width, scale);
    DrawPanicHeader(PanelX, Scale(104, scale), scale, L"Kernel Panic");
    DrawMessagePanel(PanelX, PanelY, LeftWidth, scale, msg);
    DrawRecoveryPanel(PanelX, PanelY + Scale(166, scale), LeftWidth, Scale(166, scale), scale);
    DrawRegisterSnapshot(RightX, PanelY, RightWidth, Scale(332, scale), scale, &Registers);
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

    DrawPanicBackground(width, scale);
    DrawPanicHeader(PanelX, Scale(104, scale), scale, L"CPU Exception");
    DrawInterruptFrame(PanelX, PanelY, PanelWidth, scale, frame);
    DrawPanicFooter(width, height, scale);
    HaltAfterPanic();
}
