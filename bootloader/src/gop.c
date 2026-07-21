// gop.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include <gop.h>
#include <lineosuefi.h>
LINEOS_GOP GOP;

STATIC BOOLEAN GOPSelectMode(EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutput, UINT32 width, UINT32 height)
{
    EFI_STATUS status;

    for (UINT32 ModeNumber = 0; ModeNumber < GraphicsOutput->Mode->MaxMode; ModeNumber++)
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
        UINTN size;

        status = GraphicsOutput->QueryMode(GraphicsOutput, ModeNumber, &size, &info);

        if (EFI_ERROR(status))
        {
            continue;
        }

        if (info->HorizontalResolution == width && info->VerticalResolution == height)
        {
            status = GraphicsOutput->SetMode(GraphicsOutput, ModeNumber);
            UEFIBootServices->FreePool(info);
            return !EFI_ERROR(status);
        }

        UEFIBootServices->FreePool(info);
    }

    return FALSE;
}

BOOLEAN GOPInit(VOID)
{
    EFI_STATUS status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsOutput;

    status = UEFIBootServices->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID**)&GraphicsOutput
    );

    if (EFI_ERROR(status))
    {
        return FALSE;
    }

    GOPSelectMode(GraphicsOutput, GOP_PREFERRED_WIDTH, GOP_PREFERRED_HEIGHT);

    GOP.FrameBufferBase = GraphicsOutput->Mode->FrameBufferBase;
    GOP.FrameBufferSize = GraphicsOutput->Mode->FrameBufferSize;
    GOP.ScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    GOP.ScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;
    GOP.PixelsPerScanLine = GraphicsOutput->Mode->Info->PixelsPerScanLine;
    GOP.PixelFormat = GraphicsOutput->Mode->Info->PixelFormat;
    return TRUE;
}
