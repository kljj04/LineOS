// lineosuefi.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <lineosuefi.h>

EFI_SYSTEM_TABLE  *UEFISystemTable = NULL;
EFI_BOOT_SERVICES *UEFIBootServices = NULL;
EFI_HANDLE         UEFIImageHandle = NULL;
EFI_STATUS         LineOSLastWatchdogStatus = EFI_SUCCESS;

STATIC CHAR16 HexDigit(UINTN Value)
{
    Value &= 0xF;
    return (CHAR16) (Value < 10 ? L'0' + Value : L'A' + (Value - 10));
}

STATIC VOID UInt64ToHex(UINT64 Value, CHAR16 *Buffer)
{
    Buffer[0] = L'0';
    Buffer[1] = L'x';

    for (UINTN Index = 0; Index < 16; Index++)
    {
        UINTN Shift = (15 - Index) * 4;
        Buffer[2 + Index] = HexDigit((UINTN) (Value >> Shift));
    }

    Buffer[18] = 0;
}

BOOLEAN LineOSDisableWatchdog(VOID)
{
    EFI_STATUS Status;

    if (UEFIBootServices == NULL)
    {
        return FALSE;
    }

    Status = UEFIBootServices->SetWatchdogTimer(0, 0, 0, NULL);
    LineOSLastWatchdogStatus = Status;
    return !EFI_ERROR(Status) || Status == EFI_UNSUPPORTED;
}

VOID LineOSHaltWithMessage(CONST CHAR16 *Message, EFI_STATUS Status)
{
    CHAR16 StatusText[19];

    if (UEFISystemTable != NULL && UEFISystemTable->ConOut != NULL)
    {
        UEFISystemTable->ConOut->OutputString(UEFISystemTable->ConOut, (CHAR16 *) L"\r\nLineOS boot halted: ");

        if (Message != NULL)
        {
            UEFISystemTable->ConOut->OutputString(UEFISystemTable->ConOut, (CHAR16 *) Message);
        }

        UEFISystemTable->ConOut->OutputString(UEFISystemTable->ConOut, (CHAR16 *) L" status=");
        UInt64ToHex((UINT64) Status, StatusText);
        UEFISystemTable->ConOut->OutputString(UEFISystemTable->ConOut, StatusText);
        UEFISystemTable->ConOut->OutputString(UEFISystemTable->ConOut, (CHAR16 *) L"\r\n");
    }

    while (1)
    {
        ASM("hlt");
    }
}
