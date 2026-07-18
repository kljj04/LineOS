// acpi.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Uefi.h>
#include <lineos/bootinfo.h>

BOOLEAN ACPIInit(VOID);

extern LINEOS_ACPI_RSDP *RSDP;
