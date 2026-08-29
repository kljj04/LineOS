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
#include <interrupt/idt.h>

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    UINT16 selector;

    KMemoryInit(BootInfo);

    CLI();

    ASM("mov %%cs, %0" : "=r"(selector));

    IDTInit();
    IDTLoad();

    ASM("int $3");

    HLT();
}