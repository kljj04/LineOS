// memory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include "memory.h"

LINEOS_MEMORY_MAP MemoryMap;

static EFI_MEMORY_DESCRIPTOR *Buffer = NULL;
static UINTN BufferSize = 0;
static UINTN MapKey = 0;
static EFI_HANDLE LineOSImageHandle = NULL;

void MemorySetImageHandle(EFI_HANDLE Handle)
{
    LineOSImageHandle = Handle;
}

BOOLEAN MemoryInit(void)
{
    EFI_STATUS Status;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    BufferSize = 0;

    Status = gBS->GetMemoryMap(
        &BufferSize,
        NULL,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    if(Status != EFI_BUFFER_TOO_SMALL)
        return FALSE;

    BufferSize += DescriptorSize * 16;

    Status = gBS->AllocatePool(
        EfiLoaderData,
        BufferSize,
        (VOID **)&Buffer
    );

    if(EFI_ERROR(Status))
        return FALSE;

    Status = gBS->GetMemoryMap(
        &BufferSize,
        Buffer,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    if(EFI_ERROR(Status))
        return FALSE;

    MemoryMap.MemoryMap = Buffer;
    MemoryMap.MemoryMapSize = BufferSize;
    MemoryMap.MemoryMapDescriptorSize = DescriptorSize;
    MemoryMap.MemoryMapDescriptorVersion = DescriptorVersion;

    return TRUE;
}

BOOLEAN ExitBootServices(void)
{
    EFI_STATUS Status;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    while(TRUE)
    {
        BufferSize = 0;

        Status = gBS->GetMemoryMap(
            &BufferSize,
            NULL,
            &MapKey,
            &DescriptorSize,
            &DescriptorVersion
        );

        if(Status != EFI_BUFFER_TOO_SMALL)
            return FALSE;

        BufferSize += DescriptorSize * 16;

        if(Buffer == NULL)
        {
            Status = gBS->AllocatePool(
                EfiLoaderData,
                BufferSize,
                (VOID **)&Buffer
            );

            if(EFI_ERROR(Status))
                return FALSE;
        }

        Status = gBS->GetMemoryMap(
            &BufferSize,
            Buffer,
            &MapKey,
            &DescriptorSize,
            &DescriptorVersion
        );

        if(Status == EFI_BUFFER_TOO_SMALL)
        {
            gBS->FreePool(Buffer);
            Buffer = NULL;
            continue;
        }

        if(EFI_ERROR(Status))
            return FALSE;

        MemoryMap.MemoryMap = Buffer;
        MemoryMap.MemoryMapSize = BufferSize;
        MemoryMap.MemoryMapDescriptorSize = DescriptorSize;
        MemoryMap.MemoryMapDescriptorVersion = DescriptorVersion;

        Status = gBS->ExitBootServices(
            LineOSImageHandle,
            MapKey
        );

        if(!EFI_ERROR(Status))
            return TRUE;
    }
}