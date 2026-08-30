// kernel/src/virtio/virtio_pci.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <memory/memory.h>
#include <pci/pci.h>
#include <virtio/virtio_pci.h>

#define PCI_STATUS_CAPABILITIES  0x10
#define PCI_CAPABILITY_POINTER   0x34
#define PCI_CAP_VENDOR_SPECIFIC  0x09
#define PCI_COMMAND              0x04
#define PCI_COMMAND_IO_SPACE     0x1
#define PCI_COMMAND_MEMORY_SPACE 0x2
#define PCI_COMMAND_BUS_MASTER   0x4

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG    3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4

typedef struct PACKED
{
    UINT8  CapVendor;
    UINT8  CapNext;
    UINT8  CapLength;
    UINT8  ConfigType;
    UINT8  BAR;
    UINT8  Padding[3];
    UINT32 Offset;
    UINT32 Length;
} VIRTIO_PCI_CAP;

STATIC CONST CHAR16 *LastError = L"not initialized";

STATIC VOID ResetDeviceInfo(VIRTIO_PCI_DEVICE *VirtIODevice)
{
    KMemSet(VirtIODevice, 0, sizeof(VIRTIO_PCI_DEVICE));

    VirtIODevice->CommonConfigBAR = 0xFF;
    VirtIODevice->NotifyBAR = 0xFF;
    VirtIODevice->ISRStatusBAR = 0xFF;
    VirtIODevice->DeviceConfigBAR = 0xFF;
}

STATIC VOID *GetBARAddress(VIRTIO_PCI_DEVICE *VirtIODevice, UINT8 BARIndex, UINT32 Offset)
{
    if (BARIndex >= 6 || VirtIODevice->BARs[BARIndex].Type == PCI_BAR_TYPE_NONE)
    {
        return NULL;
    }

    return (VOID *) (VirtIODevice->BARs[BARIndex].Address + Offset);
}

STATIC VOID LoadBARs(VIRTIO_PCI_DEVICE *VirtIODevice)
{
    UINT8 index;

    for (index = 0; index < 6; index++)
    {
        PCIGetBAR(VirtIODevice->PCIDevice, index, &VirtIODevice->BARs[index]);

        if (VirtIODevice->BARs[index].Type == PCI_BAR_TYPE_MEMORY64)
        {
            index++;
        }
    }
}

STATIC UINT32 ReadCapability32(PCI_DEVICE *Device, UINT8 CapabilityOffset, UINT8 FieldOffset)
{
    return PCIConfigRead32(Device, (UINT8) (CapabilityOffset + FieldOffset));
}

STATIC VOID ReadVirtIOCapability(VIRTIO_PCI_DEVICE *VirtIODevice, UINT8 CapabilityOffset)
{
    VIRTIO_PCI_CAP Capability;
    PCI_DEVICE    *Device;

    Device = VirtIODevice->PCIDevice;

    Capability.CapVendor = PCIConfigRead8(Device, CapabilityOffset);
    Capability.CapNext = PCIConfigRead8(Device, (UINT8) (CapabilityOffset + 1));
    Capability.CapLength = PCIConfigRead8(Device, (UINT8) (CapabilityOffset + 2));
    Capability.ConfigType = PCIConfigRead8(Device, (UINT8) (CapabilityOffset + 3));
    Capability.BAR = PCIConfigRead8(Device, (UINT8) (CapabilityOffset + 4));
    Capability.Offset = ReadCapability32(Device, CapabilityOffset, 8);
    Capability.Length = ReadCapability32(Device, CapabilityOffset, 12);

    if (Capability.BAR >= 6)
    {
        return;
    }

    switch (Capability.ConfigType)
    {
    case VIRTIO_PCI_CAP_COMMON_CFG:
        VirtIODevice->CommonConfigBAR = Capability.BAR;
        VirtIODevice->CommonConfigOffset = Capability.Offset;
        break;

    case VIRTIO_PCI_CAP_NOTIFY_CFG:
        VirtIODevice->NotifyBAR = Capability.BAR;
        VirtIODevice->NotifyOffset = Capability.Offset;

        if (Capability.CapLength >= 20)
        {
            VirtIODevice->NotifyMultiplier = ReadCapability32(Device, CapabilityOffset, 16);
        }

        break;

    case VIRTIO_PCI_CAP_ISR_CFG:
        VirtIODevice->ISRStatusBAR = Capability.BAR;
        VirtIODevice->ISRStatusOffset = Capability.Offset;
        break;

    case VIRTIO_PCI_CAP_DEVICE_CFG:
        VirtIODevice->DeviceConfigBAR = Capability.BAR;
        VirtIODevice->DeviceConfigOffset = Capability.Offset;
        break;

    default:
        break;
    }
}

STATIC BOOLEAN ScanCapabilities(VIRTIO_PCI_DEVICE *VirtIODevice)
{
    PCI_DEVICE *Device;
    UINT8       CapabilityOffset;
    UINT8       guard;

    Device = VirtIODevice->PCIDevice;

    if ((PCIConfigRead16(Device, 0x06) & PCI_STATUS_CAPABILITIES) == 0)
    {
        LastError = L"pci capability list missing";
        return FALSE;
    }

    CapabilityOffset = PCIConfigRead8(Device, PCI_CAPABILITY_POINTER) & 0xFC;

    for (guard = 0; CapabilityOffset != 0 && guard < 48; guard++)
    {
        UINT8 CapabilityId;

        CapabilityId = PCIConfigRead8(Device, CapabilityOffset);

        if (CapabilityId == PCI_CAP_VENDOR_SPECIFIC)
        {
            ReadVirtIOCapability(VirtIODevice, CapabilityOffset);
        }

        CapabilityOffset = PCIConfigRead8(Device, (UINT8) (CapabilityOffset + 1)) & 0xFC;
    }

    if (VirtIODevice->CommonConfigBAR == 0xFF || VirtIODevice->NotifyBAR == 0xFF || VirtIODevice->ISRStatusBAR == 0xFF || VirtIODevice->DeviceConfigBAR == 0xFF)
    {
        LastError = L"virtio pci caps incomplete";
        return FALSE;
    }

    return TRUE;
}

STATIC BOOLEAN MapCapabilities(VIRTIO_PCI_DEVICE *VirtIODevice)
{
    VirtIODevice->CommonConfig = (VIRTIO_PCI_COMMON_CONFIG *) GetBARAddress(VirtIODevice, VirtIODevice->CommonConfigBAR, VirtIODevice->CommonConfigOffset);
    VirtIODevice->NotifyBase = (VOLATILE UINT16 *) GetBARAddress(VirtIODevice, VirtIODevice->NotifyBAR, VirtIODevice->NotifyOffset);
    VirtIODevice->ISRStatus = (VOLATILE UINT8 *) GetBARAddress(VirtIODevice, VirtIODevice->ISRStatusBAR, VirtIODevice->ISRStatusOffset);
    VirtIODevice->DeviceConfig = (VOLATILE UINT8 *) GetBARAddress(VirtIODevice, VirtIODevice->DeviceConfigBAR, VirtIODevice->DeviceConfigOffset);

    if (VirtIODevice->CommonConfig == NULL || VirtIODevice->NotifyBase == NULL || VirtIODevice->ISRStatus == NULL || VirtIODevice->DeviceConfig == NULL)
    {
        LastError = L"virtio pci cap address invalid";
        return FALSE;
    }

    return TRUE;
}

STATIC VOID EnablePCIDevice(PCI_DEVICE *Device)
{
    UINT16 Command;

    Command = PCIConfigRead16(Device, PCI_COMMAND);
    Command |= PCI_COMMAND_IO_SPACE | PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;

    PCIConfigWrite16(Device, PCI_COMMAND, Command);
}

BOOLEAN VirtIOPCIInitDevice(VIRTIO_PCI_DEVICE *VirtIODevice, PCI_DEVICE *Device)
{
    if (VirtIODevice == NULL || Device == NULL)
    {
        LastError = L"virtio pci args invalid";
        return FALSE;
    }

    ResetDeviceInfo(VirtIODevice);

    VirtIODevice->PCIDevice = Device;

    LoadBARs(VirtIODevice);

    if (!ScanCapabilities(VirtIODevice))
    {
        return FALSE;
    }

    if (!MapCapabilities(VirtIODevice))
    {
        return FALSE;
    }

    EnablePCIDevice(Device);

    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtIOPCIStartDevice(VIRTIO_PCI_DEVICE *VirtIODevice)
{
    VIRTIO_PCI_COMMON_CONFIG *CommonConfig;

    if (VirtIODevice == NULL || VirtIODevice->CommonConfig == NULL)
    {
        LastError = L"virtio common cfg missing";
        return FALSE;
    }

    CommonConfig = VirtIODevice->CommonConfig;

    CommonConfig->DeviceStatus = 0;

    CommonConfig->DeviceStatus = VIRTIO_STATUS_ACKNOWLEDGE;
    CommonConfig->DeviceStatus |= VIRTIO_STATUS_DRIVER;

    CommonConfig->DriverFeatureSelect = 0;
    CommonConfig->DriverFeature = 0;

    CommonConfig->DriverFeatureSelect = 1;
    CommonConfig->DriverFeature = 1U << (VIRTIO_F_VERSION_1 - 32);

    CommonConfig->DeviceStatus |= VIRTIO_STATUS_FEATURES_OK;

    if ((CommonConfig->DeviceStatus & VIRTIO_STATUS_FEATURES_OK) == 0)
    {
        LastError = L"virtio features rejected";
        return FALSE;
    }

    LastError = L"ok";
    return TRUE;
}

VOID VirtIOPCIReadyDevice(VIRTIO_PCI_DEVICE *VirtIODevice)
{
    if (VirtIODevice == NULL || VirtIODevice->CommonConfig == NULL)
    {
        LastError = L"virtio common cfg missing";
        return;
    }

    VirtIODevice->CommonConfig->DeviceStatus |= VIRTIO_STATUS_DRIVER_OK;

    LastError = L"ok";
}

VOID VirtIOPCINotifyQueue(VIRTIO_PCI_DEVICE *VirtIODevice, UINT16 QueueIndex)
{
    VIRTIO_PCI_COMMON_CONFIG *CommonConfig;
    VOLATILE UINT16          *NotifyAddress;

    if (VirtIODevice == NULL || VirtIODevice->CommonConfig == NULL || VirtIODevice->NotifyBase == NULL)
    {
        return;
    }

    CommonConfig = VirtIODevice->CommonConfig;

    CommonConfig->QueueSelect = QueueIndex;

    NotifyAddress = (VOLATILE UINT16 *) ((UINT8 *) VirtIODevice->NotifyBase + (CommonConfig->QueueNotifyOff * VirtIODevice->NotifyMultiplier));

    *NotifyAddress = QueueIndex;
}

CONST CHAR16 *VirtIOPCIGetLastError(VOID)
{
    return LastError;
}