// Kernel/Include/Disk/GPT.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Disk/VirtIOBlock.h>

typedef struct
{
    UINT64 FirstLBA;
    UINT64 SectorCount;
    UINT64 Attributes;
    UINT8  TypeGUID[16];
} GPT_PARTITION;

BOOLEAN GPTReadPartition(VIRTIO_BLOCK_DEVICE *Device, UINT32 Index, GPT_PARTITION *Partition);
