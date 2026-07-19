// kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>
#include <arch/x86_64/cpu.h>
#include <render/framebuffer.h>
#include <render/print.h>

VOID __attribute__((ms_abi)) KMain(LINEOS_BOOT_INFO *BootInfo)
{
    UINT32 width;
    UINT32 height;
    UINT32 PanelX;
    UINT32 PanelY;
    UINT32 PanelWidth;
    UINT32 PanelHeight;

    FrameBufferInit(BootInfo);

    width = BootInfo->GOP->ScreenWidth;
    height = BootInfo->GOP->ScreenHeight;
    PanelX = 96;
    PanelY = 120;
    PanelWidth = width - 192;
    PanelHeight = height - 240;

    FillScreen(0x071018);

    FillRect(0, 0, width, 84, 0x0E263A);
    FillRect(0, 84, width, 4, 0x4BC3FF);
    FillRect(0, height - 56, width, 56, 0x081A27);
    FillRect(0, height - 60, width, 4, 0x2F8F68);

    for (UINT32 i = 0; i < 18; i++)
    {
        UINT32 OffsetX = i * 160;
        DrawLine((INT32)OffsetX, 88, (INT32)(OffsetX + 420), (INT32)(height - 60), 0x0B1F2E);
    }

    FillRect(PanelX, PanelY, PanelWidth, PanelHeight, 0x0B1620);
    DrawRect(PanelX, PanelY, PanelWidth, PanelHeight, 0x2C5368);
    DrawRect(PanelX + 8, PanelY + 8, PanelWidth - 16, PanelHeight - 16, 0x102D3E);

    FillRect(PanelX + 32, PanelY + 34, 8, 120, 0x4BC3FF);
    FillRect(PanelX + 48, PanelY + 34, 8, 120, 0x2F8F68);
    FillRect(PanelX + 64, PanelY + 34, 8, 120, 0xFFCB6B);

    KPrint(L"LineOS", PanelX + 96, PanelY + 72, 0xFFFFFF);
    KPrint(L"Open Platform Kernel Preview", PanelX + 96, PanelY + 112, 0x7DDCFF);
    KPrint(L"UEFI long mode handoff complete", PanelX + 96, PanelY + 152, 0x9FB6C3);

    KPrint(L"[ OK ] GOP framebuffer online", PanelX + 96, PanelY + 230, 0x67F0A1);
    KPrint(L"[ OK ] Font renderer online", PanelX + 96, PanelY + 266, 0x67F0A1);
    KPrint(L"[ OK ] Unicode glyph table loaded", PanelX + 96, PanelY + 302, 0x67F0A1);
    KPrint(L"[ OK ] BootInfo accepted", PanelX + 96, PanelY + 338, 0x67F0A1);

    KPrint(L"Resolution", PanelX + 96, PanelY + 422, 0x7DDCFF);
    KPrint(L"%u x %u", PanelX + 320, PanelY + 422, 0xFFFFFF, width, height);

    KPrint(L"Framebuffer", PanelX + 96, PanelY + 462, 0x7DDCFF);
    KPrint(L"%p", PanelX + 320, PanelY + 462, 0xFFFFFF, (VOID*)BootInfo->GOP->FrameBufferBase);

    KPrint(L"Memory map", PanelX + 96, PanelY + 502, 0x7DDCFF);
    KPrint(L"%u bytes", PanelX + 320, PanelY + 502, 0xFFFFFF, (UINT32)BootInfo->MemoryMap->MemoryMapSize);

    KPrint(L"한글 출력 테스트", PanelX + 96, PanelY + 590, 0xFFCB6B);
    KPrint(L"안녕하세요 LineOS 커널입니다.", PanelX + 96, PanelY + 630, 0xFFFFFF);

    FillRect(PanelX + PanelWidth - 360, PanelY + 72, 220, 220, 0x102D3E);
    DrawRect(PanelX + PanelWidth - 360, PanelY + 72, 220, 220, 0x4BC3FF);
    DrawLine((INT32)(PanelX + PanelWidth - 330), (INT32)(PanelY + 250), (INT32)(PanelX + PanelWidth - 250), (INT32)(PanelY + 110), 0x67F0A1);
    DrawLine((INT32)(PanelX + PanelWidth - 250), (INT32)(PanelY + 110), (INT32)(PanelX + PanelWidth - 170), (INT32)(PanelY + 250), 0x67F0A1);
    DrawLine((INT32)(PanelX + PanelWidth - 320), (INT32)(PanelY + 220), (INT32)(PanelX + PanelWidth - 180), (INT32)(PanelY + 220), 0xFFCB6B);

    KPrint(L"KERNEL", PanelX + PanelWidth - 350, PanelY + 338, 0x7DDCFF);
    KPrint(L"READY", PanelX + PanelWidth - 350, PanelY + 378, 0x67F0A1);

    KPrint(L"Press nothing. The kernel is enjoying its first pixels.", PanelX + 96, height - 24, 0x55798C);

    while (TRUE)
    {
        HLT();
    }
}
