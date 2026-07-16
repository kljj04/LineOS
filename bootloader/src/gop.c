// gop.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include "gop.h"
LINEOS_GOP GOP;

BOOLEAN GOPInit(void)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;

    Status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID**)&GraphicsOutput
    );

    if (EFI_ERROR(Status))
    {
        return FALSE;
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
    UINTN Size;

    Status = GraphicsOutput->QueryMode(
        GraphicsOutput,
        GraphicsOutput->Mode->Mode,
        &Size,
        &Info
    );

    if (EFI_ERROR(Status))
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
