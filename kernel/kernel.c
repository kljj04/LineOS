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

    Device = PCIGetDevice(0);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 100, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(1);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 200, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(2);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 300, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(3);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 400, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(4);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 500, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(5);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 600, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(6);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 6700, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(7);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 800, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(8);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 900, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);
    Device = PCIGetDevice(9);
    KPrint(L"Bus=%X Dev=%X Func=%X Vendor=%X Device=%X\n", 100, 1000, 0xFFFFFFFF, 50, PRETENDARD, Device->Bus, Device->Device, Device->Function, Device->VendorId, Device->DeviceId);


    VirtIOGPUFlush();

    HLT();
}