// Kernel/Include/FS/VFS/VFSEXT2.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/VFS/VFS.h>
#include <FS/EXT2/EXT2.h>

EXTERN CONST VFS_BACKEND_OPERATIONS VFSEXT2Operations;
VFS_ERROR                           VFSMountEXT2(VFS_REGISTRY *Registry, CONST CHAR8 *Name, EXT2_VOLUME *Volume, VIRTIO_BLOCK_DEVICE *Device, UINT32 GPTIndex, BOOLEAN Writable, UINT32 *Partition);
