// BootLoader/Source/ELF.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <ELF.h>
#include <LineOSUEFI.h>

#define ELF_PAGE_SIZE 4096ULL

LINEOS_KERNEL Kernel;

STATIC VOID *ELFBuffer = NULL;
STATIC UINTN ELFSize = 0;

STATIC UINT64 AlignDown(UINT64 Value, UINT64 Alignment)
{
    return Value & ~(Alignment - 1);
}

STATIC UINT64 AlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

STATIC BOOLEAN ReserveELFImage(UINT64 ImageBase, UINT64 ImageEnd)
{
    EFI_STATUS           Status;
    EFI_PHYSICAL_ADDRESS Address;
    UINTN                Pages;

    if (ImageEnd <= ImageBase)
    {
        return FALSE;
    }

    Address = (EFI_PHYSICAL_ADDRESS) ImageBase;
    Pages = (UINTN) ((ImageEnd - ImageBase) / ELF_PAGE_SIZE);

    Status = UEFIBootServices->AllocatePages(AllocateAddress, EfiLoaderData, Pages, &Address);

    return !EFI_ERROR(Status) && Address == (EFI_PHYSICAL_ADDRESS) ImageBase;
}

STATIC EFI_FILE_PROTOCOL *OpenKernelFile(EFI_HANDLE ImageHandle)
{
    EFI_STATUS                       Status;
    EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL               *Root;
    EFI_FILE_PROTOCOL               *File;

    Status = UEFIBootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **) &LoadedImage);

    if (EFI_ERROR(Status))
        return NULL;

    Status = UEFIBootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **) &FileSystem);

    if (EFI_ERROR(Status))
        return NULL;

    Status = FileSystem->OpenVolume(FileSystem, &Root);

    if (EFI_ERROR(Status))
        return NULL;

    Status = Root->Open(Root, &File, L"KERNEL\\LINEOS_KERNEL.ELF", EFI_FILE_MODE_READ, 0);

    if (EFI_ERROR(Status))
        return NULL;

    return File;
}

STATIC BOOLEAN ReadKernelFile(EFI_FILE_PROTOCOL *File)
{
    EFI_STATUS     Status;
    EFI_FILE_INFO *FileInfo;
    UINTN          InfoSize = 0;

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, NULL);

    if (Status != EFI_BUFFER_TOO_SMALL)
        return FALSE;

    Status = UEFIBootServices->AllocatePool(EfiLoaderData, InfoSize, (VOID **) &FileInfo);

    if (EFI_ERROR(Status))
        return FALSE;

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);

    if (EFI_ERROR(Status))
        return FALSE;

    ELFSize = FileInfo->FileSize;

    Status = UEFIBootServices->AllocatePool(EfiLoaderData, ELFSize, &ELFBuffer);

    if (EFI_ERROR(Status))
        return FALSE;

    Status = File->Read(File, &ELFSize, ELFBuffer);

    if (EFI_ERROR(Status))
        return FALSE;

    return TRUE;
}

STATIC BOOLEAN CheckELF(VOID)
{
    ELF64_HEADER *Header;

    Header = (ELF64_HEADER *) ELFBuffer;

    if (*(UINT32 *) Header->Ident != ELF_MAGIC32)
    {
        return FALSE;
    }

    if (Header->Machine != 0x3E)
    {
        return FALSE;
    }

    return TRUE;
}

STATIC BOOLEAN LoadELFSegments(VOID)
{
    ELF64_HEADER *Header;

    Header = (ELF64_HEADER *) ELFBuffer;

    UINT64 Base = 0xFFFFFFFFFFFFFFFFULL;
    UINT64 Size = 0;

    for (UINTN I = 0; I < Header->ProgramHeaderCount; I++)
    {
        ELF64_PROGRAM_HEADER *ProgramHeader;
        UINT64                SegmentBase;
        UINT64                SegmentEnd;

        ProgramHeader = (ELF64_PROGRAM_HEADER *) ((UINT8 *) ELFBuffer + Header->ProgramHeaderOffset + I * Header->ProgramHeaderSize);

        if (ProgramHeader->Type != PT_LOAD || ProgramHeader->MemorySize == 0)
            continue;

        SegmentBase = AlignDown(ProgramHeader->VirtualAddress, ELF_PAGE_SIZE);
        SegmentEnd = AlignUp(ProgramHeader->VirtualAddress + ProgramHeader->MemorySize, ELF_PAGE_SIZE);

        if (SegmentBase < Base)
            Base = SegmentBase;

        if (SegmentEnd > Size)
            Size = SegmentEnd;
    }

    if (!ReserveELFImage(Base, Size))
    {
        return FALSE;
    }

    SetMem((VOID *) Base, Size - Base, 0);

    for (UINTN I = 0; I < Header->ProgramHeaderCount; I++)
    {
        ELF64_PROGRAM_HEADER *ProgramHeader;

        ProgramHeader = (ELF64_PROGRAM_HEADER *) ((UINT8 *) ELFBuffer + Header->ProgramHeaderOffset + I * Header->ProgramHeaderSize);

        if (ProgramHeader->Type != PT_LOAD)
            continue;

        CopyMem((VOID *) ProgramHeader->VirtualAddress, (UINT8 *) ELFBuffer + ProgramHeader->Offset, ProgramHeader->FileSize);
    }

    Kernel.Base = Base;
    Kernel.Size = Size - Base;
    Kernel.Entry = Header->Entry;

    return TRUE;
}

BOOLEAN LoadKernel(VOID)
{
    EFI_FILE_PROTOCOL *File;

    File = OpenKernelFile(UEFIImageHandle);

    if (!File)
    {
        return FALSE;
    }

    if (!ReadKernelFile(File))
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
