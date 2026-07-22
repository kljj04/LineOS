// lineosuefi.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <lineosuefi.h>

EFI_SYSTEM_TABLE *UEFISystemTable = NULL;
EFI_BOOT_SERVICES *UEFIBootServices = NULL;
EFI_HANDLE UEFIImageHandle = NULL;
