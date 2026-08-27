// virtio_pci.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>
#include <pci/pci.h>

#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED      128

#define VIRTIO_F_VERSION_1 32

typedef struct PACKED
{
    volatile UINT32 DeviceFeatureSelect;
    volatile UINT32 DeviceFeature;
    volatile UINT32 DriverFeatureSelect;
    volatile UINT32 DriverFeature;
    volatile UINT16 MSIXConfig;
    volatile UINT16 NumQueues;
    volatile UINT8  DeviceStatus;
    volatile UINT8  ConfigGeneration;
    volatile UINT16 QueueSelect;
    volatile UINT16 QueueSize;
    volatile UINT16 QueueMSIXVector;
    volatile UINT16 QueueEnable;
    volatile UINT16 QueueNotifyOff;
    volatile UINT64 QueueDesc;
    volatile UINT64 QueueDriver;
    volatile UINT64 QueueDevice;
} VIRTIO_PCI_COMMON_CONFIG;

typedef struct
{
    PCI_DEVICE               *PCIDevice;
    PCI_BAR                   BARs[6];
    VIRTIO_PCI_COMMON_CONFIG *CommonConfig;
    volatile UINT16          *NotifyBase;
    volatile UINT8           *ISRStatus;
    volatile UINT8           *DeviceConfig;
    UINT8                     CommonConfigBAR;
    UINT8                     NotifyBAR;
    UINT8                     ISRStatusBAR;
    UINT8                     DeviceConfigBAR;
    UINT32                    CommonConfigOffset;
    UINT32                    NotifyOffset;
    UINT32                    ISRStatusOffset;
    UINT32                    DeviceConfigOffset;
    UINT32                    NotifyMultiplier;
} VIRTIO_PCI_DEVICE;

BOOLEAN       VirtIOPCIInitDevice(VIRTIO_PCI_DEVICE *VirtIODevice, UINT16 VendorId, UINT16 DeviceId);
BOOLEAN       VirtIOPCIStartDevice(VIRTIO_PCI_DEVICE *VirtIODevice);
VOID          VirtIOPCINotifyQueue(VIRTIO_PCI_DEVICE *VirtIODevice, UINT16 QueueIndex);
CONST CHAR16 *VirtIOPCIGetLastError(VOID);
