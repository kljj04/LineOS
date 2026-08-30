// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/truetype/truetype_engine.h>
#include <arch/x86_64/cpu.h>
#include <debug/panic.h>
#include <interrupt/apic.h>
#include <interrupt/idt.h>
#include <acpi/acpi.h>
#include <lineos/bootinfo.h>
#include <memory/memory.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <timer/hpet.h>
#include <timer/tsc.h>

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();

    KMemoryInit(BootInfo);
    TrueTypeInit();

    PCIInit(BootInfo);
    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    FillScreen(0x1E1E1E);
    VirtIOGPUFlush();

    IDTInit();
    IDTLoad();

    LAPICInit();

    HPETInit(BootInfo);

    TSCCalibrate();

    FillScreen(0x1E1E1E);
    PCI_DEVICE *Device;
    UINT32      i;
    UINTN       y = 100;

    for (i = 0; i < PCIGetDeviceCount(); i++)
    {
        Device = PCIGetDevice(i);

        if (Device == NULL)
        {
            continue;
        }

        KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%s (%X) Device=%X\n", 100, y, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, PCIGetVendorName(Device->VendorId), Device->VendorId, Device->DeviceId);
        y += 100;
    }

    VirtIOGPUFlush();
    HLT();
}