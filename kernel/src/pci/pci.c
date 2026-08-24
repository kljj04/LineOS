// pci.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>
#include <arch/x86_64/cpu.h>
#include <pci/pci.h>

#define ACPI_MCFG_SIGNATURE 0x4746434D
#define ACPI_RSDT_SIGNATURE 0x54445352
#define ACPI_XSDT_SIGNATURE 0x54445358
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_VENDOR_INVALID 0xFFFF
#define PCI_HEADER_TYPE_MULTI_FUNCTION 0x80

typedef struct PACKED
{
    CHAR8 Signature[8];
    UINT8 Checksum;
    CHAR8 OemId[6];
    UINT8 Revision;
    UINT32 RsdtAddress;
    UINT32 Length;
    UINT64 XsdtAddress;
    UINT8 ExtendedChecksum;
    UINT8 Reserved[3];
} ACPI_RSDP;

typedef struct PACKED
{
    UINT32 Signature;
    UINT32 Length;
    UINT8 Revision;
    UINT8 Checksum;
    CHAR8 OemId[6];
    CHAR8 OemTableId[8];
    UINT32 OemRevision;
    UINT32 CreatorId;
    UINT32 CreatorRevision;
} ACPI_SDT_HEADER;

typedef struct PACKED
{
    UINT64 BaseAddress;
    UINT16 SegmentGroup;
    UINT8 StartBus;
    UINT8 EndBus;
    UINT32 Reserved;
} ACPI_MCFG_ALLOCATION;

STATIC PCI_DEVICE Devices[PCI_MAX_DEVICES];
STATIC UINT32 DeviceCount = 0;
STATIC ACPI_SDT_HEADER *MCFG = NULL;
STATIC CONST CHAR16 *ScanMethodName = L"none";
STATIC CONST CHAR16 *LastError = L"not initialized";

typedef struct
{
    UINT16 VendorId;
    CONST CHAR16 *Name;
} PCI_VENDOR_NAME;

STATIC CONST PCI_VENDOR_NAME VendorNames[] = {
    {0x8086, L"Intel"},   {0x1AF4, L"VirtIO"}, {0x1234, L"QEMU"},    {0x1B36, L"Red Hat"},
    {0x10EC, L"Realtek"}, {0x1022, L"AMD"},    {0x1002, L"AMD/ATI"}, {0x15AD, L"VMware"},
};

STATIC BOOLEAN ChecksumValid(CONST VOID *Data, UINT32 Length)
{
    CONST UINT8 *Bytes = (CONST UINT8 *) Data;
    UINT8 Sum = 0;

    for (UINT32 Index = 0; Index < Length; Index++)
    {
        Sum = (UINT8) (Sum + Bytes[Index]);
    }

    return Sum == 0;
}

STATIC BOOLEAN RSDPValid(ACPI_RSDP *RSDP)
{
    if (RSDP == NULL || !ChecksumValid(RSDP, 20))
    {
        return FALSE;
    }

    if (RSDP->Revision >= 2 && RSDP->Length >= sizeof(ACPI_RSDP))
    {
        return ChecksumValid(RSDP, RSDP->Length);
    }

    return TRUE;
}

STATIC BOOLEAN SDTValid(ACPI_SDT_HEADER *Header)
{
    return Header != NULL && Header->Length >= sizeof(ACPI_SDT_HEADER) && ChecksumValid(Header, Header->Length);
}

STATIC ACPI_SDT_HEADER *FindTableInXSDT(ACPI_SDT_HEADER *XSDT, UINT32 Signature)
{
    UINT32 EntryCount;
    UINT64 *Entries;

    if (!SDTValid(XSDT) || XSDT->Signature != ACPI_XSDT_SIGNATURE)
    {
        return NULL;
    }

    EntryCount = (XSDT->Length - sizeof(ACPI_SDT_HEADER)) / sizeof(UINT64);
    Entries = (UINT64 *) ((UINT8 *) XSDT + sizeof(ACPI_SDT_HEADER));

    for (UINT32 Index = 0; Index < EntryCount; Index++)
    {
        ACPI_SDT_HEADER *Header = (ACPI_SDT_HEADER *) Entries[Index];

        if (SDTValid(Header) && Header->Signature == Signature)
        {
            return Header;
        }
    }

    return NULL;
}

STATIC ACPI_SDT_HEADER *FindTableInRSDT(ACPI_SDT_HEADER *RSDT, UINT32 Signature)
{
    UINT32 EntryCount;
    UINT32 *Entries;

    if (!SDTValid(RSDT) || RSDT->Signature != ACPI_RSDT_SIGNATURE)
    {
        return NULL;
    }

    EntryCount = (RSDT->Length - sizeof(ACPI_SDT_HEADER)) / sizeof(UINT32);
    Entries = (UINT32 *) ((UINT8 *) RSDT + sizeof(ACPI_SDT_HEADER));

    for (UINT32 Index = 0; Index < EntryCount; Index++)
    {
        ACPI_SDT_HEADER *Header = (ACPI_SDT_HEADER *) ((UINT64) Entries[Index]);

        if (SDTValid(Header) && Header->Signature == Signature)
        {
            return Header;
        }
    }

    return NULL;
}

STATIC ACPI_SDT_HEADER *FindACPITable(LINEOS_BOOT_INFO *BootInfo, UINT32 Signature)
{
    ACPI_RSDP *RSDP;
    ACPI_SDT_HEADER *Table;

    if (BootInfo == NULL || BootInfo->RSDP == NULL)
    {
        return NULL;
    }

    RSDP = (ACPI_RSDP *) BootInfo->RSDP;
    if (!RSDPValid(RSDP))
    {
        return NULL;
    }

    if (RSDP->Revision >= 2 && RSDP->XsdtAddress != 0)
    {
        Table = FindTableInXSDT((ACPI_SDT_HEADER *) RSDP->XsdtAddress, Signature);
        if (Table != NULL)
        {
            return Table;
        }
    }

    if (RSDP->RsdtAddress != 0)
    {
        return FindTableInRSDT((ACPI_SDT_HEADER *) ((UINT64) RSDP->RsdtAddress), Signature);
    }

    return NULL;
}

STATIC UINT64 PCIGetConfigAddress(ACPI_MCFG_ALLOCATION *Allocation, UINT8 Bus, UINT8 Device, UINT8 Function)
{
    return Allocation->BaseAddress + (((UINT64) Bus - Allocation->StartBus) << 20) + ((UINT64) Device << 15) +
           ((UINT64) Function << 12);
}

STATIC UINT8 PCIRead8(UINT64 ConfigAddress, UINT16 Offset)
{
    return *(volatile UINT8 *) (ConfigAddress + Offset);
}

STATIC UINT16 PCIRead16(UINT64 ConfigAddress, UINT16 Offset)
{
    return *(volatile UINT16 *) (ConfigAddress + Offset);
}

STATIC VOID PCIAddDevice(UINT16 Segment, UINT8 Bus, UINT8 Device, UINT8 Function, UINT16 VendorId, UINT16 DeviceId,
                         UINT8 ClassCode, UINT8 SubClass, UINT8 ProgIf)
{
    PCI_DEVICE *PCI;

    if (DeviceCount >= PCI_MAX_DEVICES)
    {
        return;
    }

    PCI = &Devices[DeviceCount++];
    PCI->Segment = Segment;
    PCI->Bus = Bus;
    PCI->Device = Device;
    PCI->Function = Function;
    PCI->VendorId = VendorId;
    PCI->DeviceId = DeviceId;
    PCI->ProgIf = ProgIf;
    PCI->SubClass = SubClass;
    PCI->ClassCode = ClassCode;
}

STATIC VOID PCIAddDeviceFromMCFG(ACPI_MCFG_ALLOCATION *Allocation, UINT8 Bus, UINT8 Device, UINT8 Function,
                                 UINT64 ConfigAddress)
{
    PCIAddDevice(Allocation->SegmentGroup, Bus, Device, Function, PCIRead16(ConfigAddress, 0x00),
                 PCIRead16(ConfigAddress, 0x02), PCIRead8(ConfigAddress, 0x0B), PCIRead8(ConfigAddress, 0x0A),
                 PCIRead8(ConfigAddress, 0x09));
}

STATIC VOID PCIScanFunctionMCFG(ACPI_MCFG_ALLOCATION *Allocation, UINT8 Bus, UINT8 Device, UINT8 Function)
{
    UINT64 ConfigAddress = PCIGetConfigAddress(Allocation, Bus, Device, Function);
    UINT16 VendorId = PCIRead16(ConfigAddress, 0x00);

    if (VendorId == PCI_VENDOR_INVALID)
    {
        return;
    }

    PCIAddDeviceFromMCFG(Allocation, Bus, Device, Function, ConfigAddress);
}

STATIC VOID PCIScanDeviceMCFG(ACPI_MCFG_ALLOCATION *Allocation, UINT8 Bus, UINT8 Device)
{
    UINT64 ConfigAddress = PCIGetConfigAddress(Allocation, Bus, Device, 0);
    UINT16 VendorId = PCIRead16(ConfigAddress, 0x00);
    UINT8 HeaderType;
    UINT8 FunctionCount;

    if (VendorId == PCI_VENDOR_INVALID)
    {
        return;
    }

    HeaderType = PCIRead8(ConfigAddress, 0x0E);
    FunctionCount = (HeaderType & PCI_HEADER_TYPE_MULTI_FUNCTION) ? 8 : 1;

    for (UINT8 Function = 0; Function < FunctionCount; Function++)
    {
        PCIScanFunctionMCFG(Allocation, Bus, Device, Function);
    }
}

STATIC VOID PCIScanBusMCFG(ACPI_MCFG_ALLOCATION *Allocation, UINT8 Bus)
{
    for (UINT8 Device = 0; Device < 32; Device++)
    {
        PCIScanDeviceMCFG(Allocation, Bus, Device);
    }
}

STATIC VOID PCIScanMCFG(ACPI_SDT_HEADER *MCFGHeader)
{
    UINT32 EntryCount;
    ACPI_MCFG_ALLOCATION *Allocations;

    EntryCount = (MCFGHeader->Length - sizeof(ACPI_SDT_HEADER) - 8) / sizeof(ACPI_MCFG_ALLOCATION);
    Allocations = (ACPI_MCFG_ALLOCATION *) ((UINT8 *) MCFGHeader + sizeof(ACPI_SDT_HEADER) + 8);

    for (UINT32 Index = 0; Index < EntryCount; Index++)
    {
        ACPI_MCFG_ALLOCATION *Allocation = &Allocations[Index];

        for (UINT32 Bus = Allocation->StartBus; Bus <= Allocation->EndBus && Bus <= 255; Bus++)
        {
            PCIScanBusMCFG(Allocation, (UINT8) Bus);
        }
    }
}

STATIC UINT32 PCIMakeIOAddress(UINT8 Bus, UINT8 Device, UINT8 Function, UINT8 Offset)
{
    return 0x80000000U | ((UINT32) Bus << 16) | ((UINT32) Device << 11) | ((UINT32) Function << 8) | (Offset & 0xFC);
}

STATIC UINT32 PCIRead32IO(UINT8 Bus, UINT8 Device, UINT8 Function, UINT8 Offset)
{
    OUTL(PCI_CONFIG_ADDRESS, PCIMakeIOAddress(Bus, Device, Function, Offset));
    return INL(PCI_CONFIG_DATA);
}

STATIC VOID PCIWrite32IO(UINT8 Bus, UINT8 Device, UINT8 Function, UINT8 Offset, UINT32 Value)
{
    OUTL(PCI_CONFIG_ADDRESS, PCIMakeIOAddress(Bus, Device, Function, Offset));
    OUTL(PCI_CONFIG_DATA, Value);
}

STATIC UINT16 PCIRead16IO(UINT8 Bus, UINT8 Device, UINT8 Function, UINT8 Offset)
{
    UINT32 Value = PCIRead32IO(Bus, Device, Function, Offset);

    return (UINT16) (Value >> ((Offset & 2) * 8));
}

STATIC UINT8 PCIRead8IO(UINT8 Bus, UINT8 Device, UINT8 Function, UINT8 Offset)
{
    UINT32 Value = PCIRead32IO(Bus, Device, Function, Offset);

    return (UINT8) (Value >> ((Offset & 3) * 8));
}

STATIC VOID PCIScanFunctionIO(UINT8 Bus, UINT8 Device, UINT8 Function)
{
    UINT16 VendorId = PCIRead16IO(Bus, Device, Function, 0x00);

    if (VendorId == PCI_VENDOR_INVALID)
    {
        return;
    }

    PCIAddDevice(0, Bus, Device, Function, VendorId, PCIRead16IO(Bus, Device, Function, 0x02),
                 PCIRead8IO(Bus, Device, Function, 0x0B), PCIRead8IO(Bus, Device, Function, 0x0A),
                 PCIRead8IO(Bus, Device, Function, 0x09));
}

STATIC VOID PCIScanDeviceIO(UINT8 Bus, UINT8 Device)
{
    UINT16 VendorId = PCIRead16IO(Bus, Device, 0, 0x00);
    UINT8 HeaderType;
    UINT8 FunctionCount;

    if (VendorId == PCI_VENDOR_INVALID)
    {
        return;
    }

    HeaderType = PCIRead8IO(Bus, Device, 0, 0x0E);
    FunctionCount = (HeaderType & PCI_HEADER_TYPE_MULTI_FUNCTION) ? 8 : 1;

    for (UINT8 Function = 0; Function < FunctionCount; Function++)
    {
        PCIScanFunctionIO(Bus, Device, Function);
    }
}

STATIC VOID PCIScanIO(VOID)
{
    for (UINT32 Bus = 0; Bus < 256; Bus++)
    {
        for (UINT8 Device = 0; Device < 32; Device++)
        {
            PCIScanDeviceIO((UINT8) Bus, Device);
        }
    }
}

BOOLEAN PCIInit(LINEOS_BOOT_INFO *BootInfo)
{
    DeviceCount = 0;
    ScanMethodName = L"none";
    LastError = L"ok";
    MCFG = FindACPITable(BootInfo, ACPI_MCFG_SIGNATURE);

    if (MCFG != NULL)
    {
        PCIScanMCFG(MCFG);
        ScanMethodName = L"mcfg";

        if (DeviceCount != 0)
        {
            return TRUE;
        }

        LastError = L"mcfg empty, used io fallback";
    }
    else
    {
        LastError = L"mcfg not found, used io fallback";
    }

    PCIScanIO();
    ScanMethodName = L"io";

    if (DeviceCount == 0)
    {
        LastError = L"no pci devices";
        return FALSE;
    }

    return TRUE;
}

UINT32 PCIGetDeviceCount(VOID)
{
    return DeviceCount;
}

PCI_DEVICE *PCIGetDevice(UINT32 Index)
{
    if (Index >= DeviceCount)
    {
        return NULL;
    }

    return &Devices[Index];
}

PCI_DEVICE *PCIFindDevice(UINT16 VendorId, UINT16 DeviceId)
{
    for (UINT32 Index = 0; Index < DeviceCount; Index++)
    {
        if (Devices[Index].VendorId == VendorId && Devices[Index].DeviceId == DeviceId)
        {
            return &Devices[Index];
        }
    }

    return NULL;
}

PCI_DEVICE *PCIFindClass(UINT8 ClassCode, UINT8 SubClass)
{
    for (UINT32 Index = 0; Index < DeviceCount; Index++)
    {
        if (Devices[Index].ClassCode == ClassCode && Devices[Index].SubClass == SubClass)
        {
            return &Devices[Index];
        }
    }

    return NULL;
}

UINT8 PCIConfigRead8(PCI_DEVICE *Device, UINT8 Offset)
{
    if (Device == NULL)
    {
        return 0xFF;
    }

    return PCIRead8IO(Device->Bus, Device->Device, Device->Function, Offset);
}

UINT16 PCIConfigRead16(PCI_DEVICE *Device, UINT8 Offset)
{
    if (Device == NULL)
    {
        return 0xFFFF;
    }

    return PCIRead16IO(Device->Bus, Device->Device, Device->Function, Offset);
}

UINT32 PCIConfigRead32(PCI_DEVICE *Device, UINT8 Offset)
{
    if (Device == NULL)
    {
        return 0xFFFFFFFF;
    }

    return PCIRead32IO(Device->Bus, Device->Device, Device->Function, Offset);
}

VOID PCIConfigWrite8(PCI_DEVICE *Device, UINT8 Offset, UINT8 Value)
{
    UINT32 Shift;
    UINT32 Mask;
    UINT32 Current;

    if (Device == NULL)
    {
        return;
    }

    Shift = (Offset & 3) * 8;
    Mask = 0xFFU << Shift;
    Current = PCIConfigRead32(Device, Offset);
    PCIConfigWrite32(Device, Offset, (Current & ~Mask) | ((UINT32) Value << Shift));
}

VOID PCIConfigWrite16(PCI_DEVICE *Device, UINT8 Offset, UINT16 Value)
{
    UINT32 Shift;
    UINT32 Mask;
    UINT32 Current;

    if (Device == NULL)
    {
        return;
    }

    Shift = (Offset & 2) * 8;
    Mask = 0xFFFFU << Shift;
    Current = PCIConfigRead32(Device, Offset);
    PCIConfigWrite32(Device, Offset, (Current & ~Mask) | ((UINT32) Value << Shift));
}

VOID PCIConfigWrite32(PCI_DEVICE *Device, UINT8 Offset, UINT32 Value)
{
    if (Device == NULL)
    {
        return;
    }

    PCIWrite32IO(Device->Bus, Device->Device, Device->Function, Offset, Value);
}

BOOLEAN PCIGetBAR(PCI_DEVICE *Device, UINT8 Index, PCI_BAR *BAR)
{
    UINT8 Offset;
    UINT32 Low;

    if (Device == NULL || BAR == NULL || Index >= 6)
    {
        return FALSE;
    }

    Offset = (UINT8) (0x10 + (Index * 4));
    Low = PCIConfigRead32(Device, Offset);
    BAR->Type = PCI_BAR_TYPE_NONE;
    BAR->Address = 0;
    BAR->Prefetchable = FALSE;

    if (Low == 0 || Low == 0xFFFFFFFF)
    {
        return FALSE;
    }

    if ((Low & 1) != 0)
    {
        BAR->Type = PCI_BAR_TYPE_IO;
        BAR->Address = Low & 0xFFFFFFFCULL;
        return TRUE;
    }

    BAR->Prefetchable = (Low & 8) != 0;
    if ((Low & 6) == 4 && Index < 5)
    {
        UINT32 High = PCIConfigRead32(Device, (UINT8) (Offset + 4));

        BAR->Type = PCI_BAR_TYPE_MEMORY64;
        BAR->Address = ((UINT64) High << 32) | (Low & 0xFFFFFFF0ULL);
        return TRUE;
    }

    BAR->Type = PCI_BAR_TYPE_MEMORY32;
    BAR->Address = Low & 0xFFFFFFF0ULL;
    return TRUE;
}

CONST CHAR16 *PCIGetScanMethodName(VOID)
{
    return ScanMethodName;
}

CONST CHAR16 *PCIGetLastError(VOID)
{
    return LastError;
}

CONST CHAR16 *PCIGetVendorName(UINT16 VendorId)
{
    for (UINT32 Index = 0; Index < sizeof(VendorNames) / sizeof(VendorNames[0]); Index++)
    {
        if (VendorNames[Index].VendorId == VendorId)
        {
            return VendorNames[Index].Name;
        }
    }

    return L"Unknown";
}
