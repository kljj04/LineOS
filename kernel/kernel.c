// kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <lineos/bootinfo.h>
#include <memory/memory.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <render/truetype/truetype_engine.h>

VOID Wait()
{
    for (VOLATILE INTN i = 0; i < 1000000000; i++)
    {
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    UINT64 CR0;
    UINT64 CR4;

    ASM("mov %%cr0, %0" : "=r"(CR0));
    CR0 = (CR0 & ~(1ULL << 2)) | (1ULL << 1);
    ASM("mov %0, %%cr0" ::"r"(CR0));

    ASM("mov %%cr4, %0" : "=r"(CR4));
    CR4 |= (1ULL << 9) | (1ULL << 10);
    ASM("mov %0, %%cr4" ::"r"(CR4));

    KMemoryInit(BootInfo);
    TrueTypeInit();
    PCIInit(BootInfo);
    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    while (TRUE)
    {
        FillScreen(0xFF0000);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0xFFFFFF,70);
        VirtIOGPUFlush();
        Wait();
        FillScreen(0x00FF00);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0xFFFFFF,70);
        VirtIOGPUFlush();
        Wait();
        FillScreen(0x0000FF);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0xFFFFFF,70);
        VirtIOGPUFlush();
        Wait();
        FillScreen(0xFF00FF);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0xFFFFFF,70);
        VirtIOGPUFlush();
        Wait();
        FillScreen(0x00FFFF);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0xFFFFFF,70);
        VirtIOGPUFlush();
        Wait();
        FillScreen(0xFFFF00);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0xFFFFFF,70);
        VirtIOGPUFlush();
        Wait();
        FillScreen(0xFFFFFF);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0x000000,70);
        VirtIOGPUFlush();
        Wait();
        FillScreen(0x000000);
        KPrint(L"한글이 깨지나 보면 되지",100,100,0xFFFFFF,70);
        VirtIOGPUFlush();
        Wait();
    }
    CLI();
    HLT();
}