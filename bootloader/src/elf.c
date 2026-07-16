// elf.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include "elf.h"

LINEOS_KERNEL Kernel;

static VOID *ELFBuffer = NULL;
static UINTN ELFSize = 0;

static EFI_FILE_PROTOCOL *OpenKernelFile(EFI_HANDLE ImageHandle)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_PROTOCOL *File;

    Status = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );

    if(EFI_ERROR(Status))
        return NULL;

    Status = gBS->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&FileSystem
    );

    if(EFI_ERROR(Status))
        return NULL;

    Status = FileSystem->OpenVolume(
        FileSystem,
        &Root
    );

    if(EFI_ERROR(Status))
        return NULL;

    Status = Root->Open(
        Root,
        &File,
        L"KERNEL\\LINEOS_KERNEL.ELF",
        EFI_FILE_MODE_READ,
        0
    );

    if(EFI_ERROR(Status))
        return NULL;

    return File;
}


static BOOLEAN ReadKernelFile(EFI_FILE_PROTOCOL *File)
{
    EFI_STATUS Status;
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = 0;

    Status = File->GetInfo(
        File,
        &gEfiFileInfoGuid,
        &InfoSize,
        NULL
    );

    if(Status != EFI_BUFFER_TOO_SMALL)
        return FALSE;

    Status = gBS->AllocatePool(
        EfiLoaderData,
        InfoSize,
        (VOID**)&FileInfo
    );

    if(EFI_ERROR(Status))
        return FALSE;

    Status = File->GetInfo(
        File,
        &gEfiFileInfoGuid,
        &InfoSize,
        FileInfo
    );

    if(EFI_ERROR(Status))
        return FALSE;

    ELFSize = FileInfo->FileSize;

    Status = gBS->AllocatePool(
        EfiLoaderData,
        ELFSize,
        &ELFBuffer
    );

    if(EFI_ERROR(Status))
        return FALSE;

    Status = File->Read(
        File,
        &ELFSize,
        ELFBuffer
    );

    if(EFI_ERROR(Status))
        return FALSE;

    return TRUE;
}


static BOOLEAN CheckELF(void)
{
    ELF64_HEADER *Header;

    Header = (ELF64_HEADER*)ELFBuffer;

    if(*(UINT32*)Header->Ident != ELF_MAGIC32)
        return FALSE;

    if(Header->Machine != 0x3E)
        return FALSE;

    return TRUE;
}


static BOOLEAN LoadELFSegments(void)
{
    ELF64_HEADER *Header;

    Header = (ELF64_HEADER*)ELFBuffer;

    UINT64 Base = 0xFFFFFFFFFFFFFFFFULL;
    UINT64 Size = 0;

    for(UINTN i = 0; i < Header->ProgramHeaderCount; i++)
    {
        ELF64_PROGRAM_HEADER *ProgramHeader;

        ProgramHeader =
            (ELF64_PROGRAM_HEADER*)
            (
                (UINT8*)ELFBuffer +
                Header->ProgramHeaderOffset +
                i * Header->ProgramHeaderSize
            );

        if(ProgramHeader->Type != PT_LOAD)
            continue;

        CopyMem(
            (VOID*)ProgramHeader->VirtualAddress,
            (UINT8*)ELFBuffer + ProgramHeader->Offset,
            ProgramHeader->FileSize
        );

        if(ProgramHeader->MemorySize > ProgramHeader->FileSize)
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

        if(ProgramHeader->VirtualAddress < Base)
            Base = ProgramHeader->VirtualAddress;

        if(
            ProgramHeader->VirtualAddress +
            ProgramHeader->MemorySize > Size
        )
        {
            Size =
                ProgramHeader->VirtualAddress +
                ProgramHeader->MemorySize;
        }
    }

    Kernel.Base = Base;
    Kernel.Size = Size - Base;
    Kernel.Entry = Header->Entry;

    return TRUE;
}


BOOLEAN LoadKernel(void)
{
    EFI_FILE_PROTOCOL *File;

    File = OpenKernelFile(gImageHandle);

    if(!File)
        return FALSE;

    if(!ReadKernelFile(File))
        return FALSE;

    if(!CheckELF())
        return FALSE;

    if(!LoadELFSegments())
        return FALSE;

    return TRUE;
}