// gop.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include "gop.h"
#include "lineosuefi.h"
LINEOS_GOP GOP;

BOOLEAN GOPInit(VOID)
{
    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;

    status = UEFIBootServices->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID**)&GraphicsOutput
    );

    if (EFI_ERROR(status))
    {
        return FALSE;
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    UINTN size;

    status = GraphicsOutput->QueryMode(
        GraphicsOutput,
        GraphicsOutput->Mode->Mode,
        &size,
        &info
    );

    if (EFI_ERROR(status))
    {
        return FALSE;
    }

    GOP.FrameBufferBase = GraphicsOutput->Mode->FrameBufferBase;
    GOP.FrameBufferSize = GraphicsOutput->Mode->FrameBufferSize;
    GOP.ScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    GOP.ScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    GOP.PixelsPerScanLine = GraphicsOutput->Mode->Info->PixelsPerScanLine;
    GOP.PixelFormat = GraphicsOutput->Mode->Info->PixelFormat;
    return TRUE;
}
