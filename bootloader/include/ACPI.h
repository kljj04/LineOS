// bootloader/include/acpi.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/ACPI.h>
#include <LineOS/BootInfo.h>

BOOLEAN ACPIInit(VOID);

EXTERN ACPI_RSDP *RSDP;
