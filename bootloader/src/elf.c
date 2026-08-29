// bootloader/src/elf.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <elf.h>
#include <lineosuefi.h>

#define ELF_PAGE_SIZE 4096ULL

LINEOS_KERNEL kernel;

STATIC VOID *ELFBuffer = NULL;
STATIC UINTN ELFSize = 0;

STATIC UINT64 AlignDown(UINT64 value, UINT64 alignment)
{
    return value & ~(alignment - 1);
}

STATIC UINT64 AlignUp(UINT64 value, UINT64 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

STATIC BOOLEAN ReserveELFImage(UINT64 ImageBase, UINT64 ImageEnd)
{
    EFI_STATUS           status;
    EFI_PHYSICAL_ADDRESS address;
    UINTN                pages;

    if (ImageEnd <= ImageBase)
    {
        return FALSE;
    }

    address = (EFI_PHYSICAL_ADDRESS) ImageBase;
    pages = (UINTN) ((ImageEnd - ImageBase) / ELF_PAGE_SIZE);

    status = UEFIBootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &address);
    return !EFI_ERROR(status) && address == (EFI_PHYSICAL_ADDRESS) ImageBase;
}

STATIC EFI_FILE_PROTOCOL *OpenKernelFile(EFI_HANDLE ImageHandle)
{
    EFI_STATUS                       status;
    EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL               *root;
    EFI_FILE_PROTOCOL               *file;

    status = UEFIBootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **) &LoadedImage);

    if (EFI_ERROR(status))
        return NULL;

    status = UEFIBootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **) &FileSystem);

    if (EFI_ERROR(status))
        return NULL;

    status = FileSystem->OpenVolume(FileSystem, &root);

    if (EFI_ERROR(status))
        return NULL;

    status = root->Open(root, &file, L"KERNEL\\LINEOS_KERNEL.ELF", EFI_FILE_MODE_READ, 0);

    if (EFI_ERROR(status))
        return NULL;

    return file;
}

STATIC BOOLEAN ReadKernelFile(EFI_FILE_PROTOCOL *file)
{
    EFI_STATUS     status;
    EFI_FILE_INFO *FileInfo;
    UINTN          InfoSize = 0;

    status = file->GetInfo(file, &gEfiFileInfoGuid, &InfoSize, NULL);

    if (status != EFI_BUFFER_TOO_SMALL)
        return FALSE;

    status = UEFIBootServices->AllocatePool(EfiLoaderData, InfoSize, (VOID **) &FileInfo);

    if (EFI_ERROR(status))
        return FALSE;

    status = file->GetInfo(file, &gEfiFileInfoGuid, &InfoSize, FileInfo);

    if (EFI_ERROR(status))
        return FALSE;

    ELFSize = FileInfo->FileSize;

    status = UEFIBootServices->AllocatePool(EfiLoaderData, ELFSize, &ELFBuffer);

    if (EFI_ERROR(status))
        return FALSE;

    status = file->Read(file, &ELFSize, ELFBuffer);

    if (EFI_ERROR(status))
        return FALSE;

    return TRUE;
}

STATIC BOOLEAN CheckELF(VOID)
{
    ELF64_HEADER *header;

    header = (ELF64_HEADER *) ELFBuffer;

    if (*(UINT32 *) header->Ident != ELF_MAGIC32)
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
    ELF64_HEADER *header;

    header = (ELF64_HEADER *) ELFBuffer;

    UINT64 base = 0xFFFFFFFFFFFFFFFFULL;
    UINT64 size = 0;

    for (UINTN i = 0; i < header->ProgramHeaderCount; i++)
    {
        ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64                SegmentBase;
        UINT64                SegmentEnd;

        ProgramHeader = (ELF64_PROGRAM_HEADER *) ((UINT8 *) ELFBuffer + header->ProgramHeaderOffset + i * header->ProgramHeaderSize);

        if (ProgramHeader->Type != PT_LOAD || ProgramHeader->MemorySize == 0)
            continue;

        SegmentBase = AlignDown(ProgramHeader->VirtualAddress, ELF_PAGE_SIZE);
        SegmentEnd = AlignUp(ProgramHeader->VirtualAddress + ProgramHeader->MemorySize, ELF_PAGE_SIZE);

        if (SegmentBase < base)
            base = SegmentBase;

        if (SegmentEnd > size)
            size = SegmentEnd;
    }

    if (!ReserveELFImage(base, size))
    {
        return FALSE;
    }

    SetMem((VOID *) base, size - base, 0);

    for (UINTN i = 0; i < header->ProgramHeaderCount; i++)
    {
        ELF64_PROGRAM_HEADER *ProgramHeader;

        ProgramHeader = (ELF64_PROGRAM_HEADER *) ((UINT8 *) ELFBuffer + header->ProgramHeaderOffset + i * header->ProgramHeaderSize);

        if (ProgramHeader->Type != PT_LOAD)
            continue;

        CopyMem((VOID *) ProgramHeader->VirtualAddress, (UINT8 *) ELFBuffer + ProgramHeader->Offset, ProgramHeader->FileSize);
    }

    kernel.Base = base;
    kernel.Size = size - base;
    kernel.Entry = header->Entry;

    return TRUE;
}

BOOLEAN LoadKernel(VOID)
{
    EFI_FILE_PROTOCOL *file;

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
