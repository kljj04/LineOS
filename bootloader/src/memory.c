// memory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include "memory.h"
#include "lineosuefi.h"

LINEOS_MEMORY_MAP MemoryMap;

STATIC EFI_MEMORY_DESCRIPTOR *buffer = NULL;
STATIC UINTN BufferSize = 0;
STATIC UINTN MapKey = 0;
STATIC EFI_HANDLE LineOSImageHandle = NULL;

VOID MemorySetImageHandle(EFI_HANDLE Handle)
{
    LineOSImageHandle = Handle;
}

BOOLEAN MemoryInit(VOID)
{
    EFI_STATUS status;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    BufferSize = 0;

    status = UEFIBootServices->GetMemoryMap(&BufferSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);

    if(status != EFI_BUFFER_TOO_SMALL)
    {
        return FALSE;
    }

    BufferSize += DescriptorSize * 64;

    status = UEFIBootServices->AllocatePool(EfiLoaderData, BufferSize, (VOID **)&buffer);

    if(EFI_ERROR(status))
    {
        return FALSE;
    }

    status = UEFIBootServices->GetMemoryMap(&BufferSize, buffer, &MapKey, &DescriptorSize, &DescriptorVersion);

    if(EFI_ERROR(status))
    {
        return FALSE;
    }

    MemoryMap.MemoryMap = buffer;
    MemoryMap.MemoryMapSize = BufferSize;
    MemoryMap.MemoryMapDescriptorSize = DescriptorSize;
    MemoryMap.MemoryMapDescriptorVersion = DescriptorVersion;

    return TRUE;
}

BOOLEAN ExitBootServices(VOID)
{
    EFI_STATUS status;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    while(TRUE)
    {
        BufferSize = 0;

        status = UEFIBootServices->GetMemoryMap(&BufferSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);

        if(status != EFI_BUFFER_TOO_SMALL)
        {
            return FALSE;
        }

        BufferSize += DescriptorSize * 64;

        if(buffer != NULL)
        {
            UEFIBootServices->FreePool(buffer);
            buffer = NULL;
        }

        status = UEFIBootServices->AllocatePool(EfiLoaderData, BufferSize, (VOID **)&buffer);

        if(EFI_ERROR(status))
        {
            return FALSE;
        }

        status = UEFIBootServices->GetMemoryMap(&BufferSize, buffer, &MapKey, &DescriptorSize, &DescriptorVersion);

        if(status == EFI_BUFFER_TOO_SMALL)
        {
            continue;
        }

        if(EFI_ERROR(status))
        {
            return FALSE;
        }

        MemoryMap.MemoryMap = buffer;
        MemoryMap.MemoryMapSize = BufferSize;
        MemoryMap.MemoryMapDescriptorSize = DescriptorSize;
        MemoryMap.MemoryMapDescriptorVersion = DescriptorVersion;

        status = UEFIBootServices->ExitBootServices(LineOSImageHandle, MapKey);

        if(!EFI_ERROR(status))
        {
            return TRUE;
        }

        if(status != EFI_INVALID_PARAMETER)
        {
            return FALSE;
        }
    }
}
