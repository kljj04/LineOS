// pci.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

#define PCI_MAX_DEVICES 32

typedef enum
{
    PCI_BAR_TYPE_NONE,
    PCI_BAR_TYPE_IO,
    PCI_BAR_TYPE_MEMORY32,
    PCI_BAR_TYPE_MEMORY64
} PCI_BAR_TYPE;

typedef struct
{
    UINT16 Segment;
    UINT8  Bus;
    UINT8  Device;
    UINT8  Function;
    UINT16 VendorId;
    UINT16 DeviceId;
    UINT8  ClassCode;
    UINT8  SubClass;
    UINT8  ProgIf;
} PCI_DEVICE;

typedef struct
{
    PCI_BAR_TYPE Type;
    UINT64       Address;
    BOOLEAN      Prefetchable;
} PCI_BAR;

BOOLEAN       PCIInit(LINEOS_BOOT_INFO *BootInfo);
UINT32        PCIGetDeviceCount(VOID);
PCI_DEVICE   *PCIGetDevice(UINT32 Index);
PCI_DEVICE   *PCIFindDevice(UINT16 VendorId, UINT16 DeviceId);
PCI_DEVICE   *PCIFindClass(UINT8 ClassCode, UINT8 SubClass);
UINT8         PCIConfigRead8(PCI_DEVICE *Device, UINT8 Offset);
UINT16        PCIConfigRead16(PCI_DEVICE *Device, UINT8 Offset);
UINT32        PCIConfigRead32(PCI_DEVICE *Device, UINT8 Offset);
VOID          PCIConfigWrite8(PCI_DEVICE *Device, UINT8 Offset, UINT8 Value);
VOID          PCIConfigWrite16(PCI_DEVICE *Device, UINT8 Offset, UINT16 Value);
VOID          PCIConfigWrite32(PCI_DEVICE *Device, UINT8 Offset, UINT32 Value);
BOOLEAN       PCIGetBAR(PCI_DEVICE *Device, UINT8 Index, PCI_BAR *BAR);
CONST CHAR16 *PCIGetScanMethodName(VOID);
CONST CHAR16 *PCIGetLastError(VOID);
CONST CHAR16 *PCIGetVendorName(UINT16 VendorId);
