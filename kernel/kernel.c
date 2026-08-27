// kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <debug/debug.h>
#include <lineos/bootinfo.h>
#include <memory/memory.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype_engine.h>

STATIC VOID DrawVirtIOTestWindow(UINT32 Width, UINT32 Height)
{
    UINT32 WindowWidth = Width - 160;
    UINT32 WindowHeight = Height - 160;
    UINT32 WindowX = (Width - WindowWidth) / 2;
    UINT32 WindowY = (Height - WindowHeight) / 2;
    UINT32 x = WindowX + 48;
    UINT32 y = WindowY + 96;

    VirtIOGPUFill(0x000B1020);
    VirtIOGPUFillRect(WindowX, WindowY, WindowWidth, WindowHeight, 0x00101522);
    VirtIOGPUFillRect(WindowX, WindowY, WindowWidth, 6, 0x0022D3EE);
    VirtIOGPUFillRect(WindowX, WindowY + WindowHeight - 6, WindowWidth, 6, 0x0022D3EE);
    VirtIOGPUFillRect(WindowX, WindowY, 6, WindowHeight, 0x0022D3EE);
    VirtIOGPUFillRect(WindowX + WindowWidth - 6, WindowY, 6, WindowHeight, 0x0022D3EE);
    VirtIOGPUFillRect(WindowX, WindowY, WindowWidth, 48, 0x00263B55);
    VirtIOGPUFillRect(WindowX + 24, WindowY + 16, 18, 18, 0x00FF6666);
    VirtIOGPUFillRect(WindowX + 54, WindowY + 16, 18, 18, 0x00FACC15);
    VirtIOGPUFillRect(WindowX + 84, WindowY + 16, 18, 18, 0x0022C55E);

    TrueTypeSelectFont(TRUE_TYPE_FONT_PRETENDARD);
    DrawTrueTypeText(L"LineOS TrueType", x, y, 0x0022D3EE, 42);
    DrawTrueTypeText(L"ASCII: ABCDEFGHIJKLMNOPQRSTUVWXYZ", x, y + 62, 0x00E5E7EB, 28);
    DrawTrueTypeText(L"ascii: abcdefghijklmnopqrstuvwxyz", x, y + 106, 0x00CBD5E1, 28);
    DrawTrueTypeText(L"digits: 0123456789  symbols: !@#$%^&*()[]{}<>", x, y + 150, 0x0094A3B8, 24);
    DrawTrueTypeText(L"한글: 프리텐다드 세미볼드 렌더링 OK", x, y + 196, 0x00F8FAFC, 30);

    TrueTypeSelectFont(TRUE_TYPE_FONT_JETBRAINS_MONO);
    DrawTrueTypeText(L"JetBrains Mono Nerd Font", x, y + 260, 0x0022D3EE, 30);
    DrawTrueTypeText(L"ASCII: 0xDEADBEEF -> /home/kljj04/LineOS", x, y + 306, 0x00E5E7EB, 24);
    DrawTrueTypeText(L"Nerd: \uf120  \uf07b  \uf15b  \uf121  \ue5ff  \ue7a2  \ue7ad  \ue73c", x, y + 356, 0x00FACC15,
                     34);
    TrueTypeSelectFont(TRUE_TYPE_FONT_PRETENDARD);
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;
    UINT32 Width;
    UINT32 Height;

    UINT64 CR0;
    UINT64 CR4;

    ASM("mov %%cr0, %0" : "=r"(CR0));
    CR0 &= ~(1ULL << 2);
    CR0 |=  (1ULL << 1);
    ASM("mov %0, %%cr0" :: "r"(CR0));

    ASM("mov %%cr4, %0" : "=r"(CR4));
    CR4 |= (1ULL << 9);
    CR4 |= (1ULL << 10);
    ASM("mov %0, %%cr4" :: "r"(CR4));

    DebugWriteLine("LineOS kernel start");

    if (!KMemoryInit(BootInfo))
    {
        DebugWriteLine("memory init failed");
        CLI();
        HLT();
    }
    DebugWriteLine("memory init ok");

    if (!TrueTypeInit())
    {
        DebugWriteLine("truetype init failed");
    }
    else
    {
        DebugWriteLine("truetype init ok");
    }

    if (!PCIInit(BootInfo))
    {
        DebugWriteLine("pci init failed");
        CLI();
        HLT();
    }
    DebugWrite("pci init ok count=");
    DebugWriteDec(PCIGetDeviceCount());
    DebugWrite("\n");

    if (!VirtIOGPUInit())
    {
        DebugWriteLine("virtio gpu init failed");
        GPU = VirtIOGPUGetInfo();
        DebugWrite("stage=");
        DebugWriteWide(GPU->Debug.LastStage);
        DebugWrite(" cmd=");
        DebugWriteHex(GPU->Debug.LastCommand);
        DebugWrite(" resp=");
        DebugWriteHex(GPU->Debug.LastResponse);
        DebugWrite(" err=");
        DebugWriteWide(VirtIOGPUGetLastError());
        DebugWrite("\n");
        DebugWrite("flags init=");
        DebugWriteDec(GPU->Debug.InitOK);
        DebugWrite(" start=");
        DebugWriteDec(GPU->Debug.StartOK);
        DebugWrite(" q0=");
        DebugWriteDec(GPU->Debug.ControlQueueOK);
        DebugWrite(" q1=");
        DebugWriteDec(GPU->Debug.CursorQueueOK);
        DebugWrite(" info=");
        DebugWriteDec(GPU->Debug.DisplayInfoOK);
        DebugWrite("\n");
        CLI();
        HLT();
    }
    DebugWriteLine("virtio gpu init ok");

    GPU = VirtIOGPUGetInfo();
    Width = GPU->DisplayInfo.Displays[0].Rect.Width;
    Height = GPU->DisplayInfo.Displays[0].Rect.Height;
    DebugWrite("scanout0 ");
    DebugWriteDec(Width);
    DebugWrite("x");
    DebugWriteDec(Height);
    DebugWrite("\n");

    if (VirtIOGPUCreateFrameBuffer(Width, Height))
    {
        DebugWrite("virtio fb create ok addr=");
        DebugWriteHex((UINT64) GPU->FrameBuffer);
        DebugWrite("\n");
        DrawVirtIOTestWindow(GPU->FrameBufferWidth, GPU->FrameBufferHeight);
        DebugWrite("pixels p0=");
        DebugWriteHex(VirtIOGPUReadPixel(0, 0));
        DebugWrite(" pc=");
        DebugWriteHex(VirtIOGPUReadPixel(GPU->FrameBufferWidth / 2, GPU->FrameBufferHeight / 2));
        DebugWrite("\n");
        if (VirtIOGPUFlush())
        {
            DebugWrite("virtio flush ok cmd=");
            DebugWriteHex(GPU->Debug.LastCommand);
            DebugWrite(" resp=");
            DebugWriteHex(GPU->Debug.LastResponse);
            DebugWrite("\n");
        }
        else
        {
            DebugWrite("virtio flush failed cmd=");
            DebugWriteHex(GPU->Debug.LastCommand);
            DebugWrite(" resp=");
            DebugWriteHex(GPU->Debug.LastResponse);
            DebugWrite("\n");
        }
    }
    else
    {
        DebugWrite("virtio fb create failed cmd=");
        DebugWriteHex(GPU->Debug.LastCommand);
        DebugWrite(" resp=");
        DebugWriteHex(GPU->Debug.LastResponse);
        DebugWrite("\n");
    }

    CLI();
    HLT();
}
