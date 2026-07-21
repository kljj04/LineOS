// elf.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <elf.h>
#include <lineosuefi.h>

LINEOS_KERNEL kernel;

STATIC VOID* ELFBuffer = NULL;
STATIC UINTN ELFSize = 0;

STATIC EFI_FILE_PROTOCOL* OpenKernelFile(EFI_HANDLE ImageHandle)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
    EFI_FILE_PROTOCOL* root;
    EFI_FILE_PROTOCOL* file;

    status = UEFIBootServices->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );

    if (EFI_ERROR(status))
        return NULL;

    status = UEFIBootServices->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&FileSystem
    );

    if (EFI_ERROR(status))
        return NULL;

    status = FileSystem->OpenVolume(
        FileSystem,
        &root
    );

    if (EFI_ERROR(status))
        return NULL;

    status = root->Open(
        root,
        &file,
        L"KERNEL\\LINEOS_KERNEL.ELF",
        EFI_FILE_MODE_READ,
        0
    );

    if (EFI_ERROR(status))
        return NULL;

    return file;
}


STATIC BOOLEAN ReadKernelFile(EFI_FILE_PROTOCOL* file)
{
    EFI_STATUS status;
    EFI_FILE_INFO* fileInfo;
    UINTN InfoSize = 0;

    status = file->GetInfo(
        file,
        &gEfiFileInfoGuid,
        &InfoSize,
        NULL
    );

    if (status != EFI_BUFFER_TOO_SMALL)
        return FALSE;

    status = UEFIBootServices->AllocatePool(
        EfiLoaderData,
        InfoSize,
        (VOID**)&fileInfo
    );

    if (EFI_ERROR(status))
        return FALSE;

    status = file->GetInfo(
        file,
        &gEfiFileInfoGuid,
        &InfoSize,
        fileInfo
    );

    if (EFI_ERROR(status))
        return FALSE;

    ELFSize = fileInfo->FileSize;

    status = UEFIBootServices->AllocatePool(
        EfiLoaderData,
        ELFSize,
        &ELFBuffer
    );

    if (EFI_ERROR(status))
        return FALSE;

    status = file->Read(
        file,
        &ELFSize,
        ELFBuffer
    );

    if (EFI_ERROR(status))
        return FALSE;

    return TRUE;
}


STATIC BOOLEAN CheckELF(VOID)
{
    ELF64_HEADER* header;

    header = (ELF64_HEADER*)ELFBuffer;

    if (*(UINT32*)header->Ident != ELF_MAGIC32)
    {
        return FALSE;
    }

    if (header->Machine != 0x3E)
    {
        return FALSE;
    }

    return TRUE;
}


STATIC BOOLEAN LoadELFSegments(VOID)
{
    ELF64_HEADER* header;

    header = (ELF64_HEADER*)ELFBuffer;

    UINT64 base = 0xFFFFFFFFFFFFFFFFULL;
    UINT64 size = 0;

    for (UINTN i = 0; i < header->ProgramHeaderCount; i++)
    {
        ELF64_PROGRAM_HEADER* ProgramHeader;

        ProgramHeader =
            (ELF64_PROGRAM_HEADER*)
            (
                (UINT8*)ELFBuffer +
                header->ProgramHeaderOffset +
                i * header->ProgramHeaderSize
            );

        if (ProgramHeader->Type != PT_LOAD)
            continue;

        CopyMem(
            (VOID*)ProgramHeader->VirtualAddress,
            (UINT8*)ELFBuffer + ProgramHeader->Offset,
            ProgramHeader->FileSize
        );

        if (ProgramHeader->MemorySize > ProgramHeader->FileSize)
        {
            SetMem(
                (VOID*)
                (
                    ProgramHeader->VirtualAddress +
                    ProgramHeader->FileSize
                ),
                ProgramHeader->MemorySize -
                ProgramHeader->FileSize,
                0
            );
        }

        if (ProgramHeader->VirtualAddress < base)
            base = ProgramHeader->VirtualAddress;

        if (
            ProgramHeader->VirtualAddress +
            ProgramHeader->MemorySize > size
        )
        {
            size =
                ProgramHeader->VirtualAddress +
                ProgramHeader->MemorySize;
        }
    }

    kernel.Base = base;
    kernel.Size = size - base;
    kernel.Entry = header->Entry;

    return TRUE;
}


BOOLEAN LoadKernel(VOID)
{
    EFI_FILE_PROTOCOL* file;

    file = OpenKernelFile(UEFIImageHandle);

    if (!file)
    {
        return FALSE;
    }

    if (!ReadKernelFile(file))
    {
        return FALSE;
    }

    if (!CheckELF())
    {
        return FALSE;
    }

    if (!LoadELFSegments())
    {
        return FALSE;
    }

    return TRUE;
}
