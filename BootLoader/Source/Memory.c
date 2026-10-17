// bootloader/src/memory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <LineOSUEFI.h>
#include <Memory.h>

LINEOS_MEMORY_MAP MemoryMap;

STATIC EFI_MEMORY_DESCRIPTOR *Buffer = NULL;
STATIC UINTN                  BufferSize = 0;
STATIC UINTN                  MapKey = 0;
STATIC EFI_HANDLE             LineOSImageHandle = NULL;

VOID MemorySetImageHandle(EFI_HANDLE Handle)
{
    LineOSImageHandle = Handle;
}

BOOLEAN MemoryInit(VOID)
{
    EFI_STATUS Status;
    UINTN      DescriptorSize;
    UINT32     DescriptorVersion;

    BufferSize = 0;

    Status = UEFIBootServices->GetMemoryMap(&BufferSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);

    if (Status != EFI_BUFFER_TOO_SMALL)
    {
        return FALSE;
    }

    BufferSize += DescriptorSize * 64;

    Status = UEFIBootServices->AllocatePool(EfiLoaderData, BufferSize, (VOID **) &Buffer);

    if (EFI_ERROR(Status))
    {
        return FALSE;
    }

    Status = UEFIBootServices->GetMemoryMap(&BufferSize, Buffer, &MapKey, &DescriptorSize, &DescriptorVersion);

    if (EFI_ERROR(Status))
    {
        return FALSE;
    }

    MemoryMap.MemoryMap = Buffer;
    MemoryMap.MemoryMapSize = BufferSize;
    MemoryMap.MemoryMapDescriptorSize = DescriptorSize;
    MemoryMap.MemoryMapDescriptorVersion = DescriptorVersion;

    return TRUE;
}

BOOLEAN ExitBootServices(VOID)
{
    EFI_STATUS Status;
    UINTN      DescriptorSize;
    UINT32     DescriptorVersion;

    while (TRUE)
    {
        BufferSize = 0;

        Status = UEFIBootServices->GetMemoryMap(&BufferSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);

        if (Status != EFI_BUFFER_TOO_SMALL)
        {
            return FALSE;
        }

        BufferSize += DescriptorSize * 64;

        if (Buffer != NULL)
        {
            UEFIBootServices->FreePool(Buffer);
            Buffer = NULL;
        }

        Status = UEFIBootServices->AllocatePool(EfiLoaderData, BufferSize, (VOID **) &Buffer);

        if (EFI_ERROR(Status))
        {
            return FALSE;
        }

        Status = UEFIBootServices->GetMemoryMap(&BufferSize, Buffer, &MapKey, &DescriptorSize, &DescriptorVersion);

        if (Status == EFI_BUFFER_TOO_SMALL)
        {
            continue;
        }

        if (EFI_ERROR(Status))
        {
            return FALSE;
        }

        MemoryMap.MemoryMap = Buffer;
        MemoryMap.MemoryMapSize = BufferSize;
        MemoryMap.MemoryMapDescriptorSize = DescriptorSize;
        MemoryMap.MemoryMapDescriptorVersion = DescriptorVersion;

        Status = UEFIBootServices->ExitBootServices(LineOSImageHandle, MapKey);

        if (!EFI_ERROR(Status))
        {
            return TRUE;
        }

        if (Status != EFI_INVALID_PARAMETER)
        {
            return FALSE;
        }
    }
}
