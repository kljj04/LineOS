// bootloader/src/GOP.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <GOP.h>
#include <LineOSUEFI.h>
LINEOS_GOP GOP;

STATIC BOOLEAN GOPSelectMode(EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput, UINT32 Width, UINT32 Height)
{
    EFI_STATUS Status;

    for (UINT32 ModeNumber = 0; ModeNumber < GraphicsOutput->Mode->MaxMode; ModeNumber++)
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
        UINTN                                 Size;

        Status = GraphicsOutput->QueryMode(GraphicsOutput, ModeNumber, &Size, &Info);

        if (EFI_ERROR(Status))
        {
            continue;
        }

        if (Info->HorizontalResolution == Width && Info->VerticalResolution == Height)
        {
            Status = GraphicsOutput->SetMode(GraphicsOutput, ModeNumber);
            UEFIBootServices->FreePool(Info);
            return !EFI_ERROR(Status);
        }

        UEFIBootServices->FreePool(Info);
    }

    return FALSE;
}

BOOLEAN GOPInit(VOID)
{
    EFI_STATUS                    Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;

    Status = UEFIBootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **) &GraphicsOutput);

    if (EFI_ERROR(Status))
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
