// guid.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Guid/FileInfo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include "guid.h"


EFI_GUID gEfiAcpi20TableGuid = EFI_ACPI_20_TABLE_GUID;

EFI_GUID gEfiFileInfoGuid = EFI_FILE_INFO_ID;

EFI_GUID gEfiLoadedImageProtocolGuid =
    EFI_LOADED_IMAGE_PROTOCOL_GUID;

EFI_GUID gEfiSimpleFileSystemProtocolGuid =
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

EFI_GUID gEfiGraphicsOutputProtocolGuid =
    EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;