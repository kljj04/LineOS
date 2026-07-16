// kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>

void kernel_main(LINEOS_BOOT_INFO *BootInfo)
{
    volatile UINT32 *Framebuffer;

    Framebuffer =
        (UINT32*)BootInfo->GOP->FrameBufferBase;

    Framebuffer[0] = 0x00FFFFFF;

    while(1)
    {
        __asm__ volatile("hlt");
    }
}