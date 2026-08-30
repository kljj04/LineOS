// kernel/include/virtio/virtio_pci.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <pci/pci.h>

#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED      128

#define VIRTIO_F_VERSION_1 32

typedef struct PACKED
{
    VOLATILE UINT32 DeviceFeatureSelect;
    VOLATILE UINT32 DeviceFeature;
    VOLATILE UINT32 DriverFeatureSelect;
    VOLATILE UINT32 DriverFeature;
    VOLATILE UINT16 MSIXConfig;
    VOLATILE UINT16 NumQueues;
    VOLATILE UINT8  DeviceStatus;
    VOLATILE UINT8  ConfigGeneration;
    VOLATILE UINT16 QueueSelect;
    VOLATILE UINT16 QueueSize;
    VOLATILE UINT16 QueueMSIXVector;
    VOLATILE UINT16 QueueEnable;
    VOLATILE UINT16 QueueNotifyOff;
    VOLATILE UINT64 QueueDesc;
    VOLATILE UINT64 QueueDriver;
    VOLATILE UINT64 QueueDevice;
} VIRTIO_PCI_COMMON_CONFIG;

typedef struct
{
    PCI_DEVICE               *PCIDevice;
    PCI_BAR                   BARs[6];
    VIRTIO_PCI_COMMON_CONFIG *CommonConfig;
    VOLATILE UINT16          *NotifyBase;
    VOLATILE UINT8           *ISRStatus;
    VOLATILE UINT8           *DeviceConfig;
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

BOOLEAN       VirtIOPCIInitDevice(VIRTIO_PCI_DEVICE *VirtIODevice, PCI_DEVICE *Device);
BOOLEAN       VirtIOPCIStartDevice(VIRTIO_PCI_DEVICE *VirtIODevice);
VOID          VirtIOPCIReadyDevice(VIRTIO_PCI_DEVICE *VirtIODevice);
VOID          VirtIOPCINotifyQueue(VIRTIO_PCI_DEVICE *VirtIODevice, UINT16 QueueIndex);
CONST CHAR16 *VirtIOPCIGetLastError(VOID);
