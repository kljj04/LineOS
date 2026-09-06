// kernel/include/multicore/smp.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>
#include <lineos/typeinfo.h>

#define SMP_MAX_CPUS       256
#define SMP_AP_STACK_PAGES 16

#define SMP_TRAMPOLINE_ADDRESS       0x8000ULL
#define SMP_TRAMPOLINE_CR3_ADDRESS   0x8F00ULL
#define SMP_TRAMPOLINE_STACK_ADDRESS 0x8F08ULL
#define SMP_TRAMPOLINE_ENTRY_ADDRESS 0x8F10ULL
#define SMP_TRAMPOLINE_CPUID_ADDRESS 0x8F18ULL

typedef struct CPU_INFO
{
    UINT32           CPUID;
    UINT32           APICID;
    BOOLEAN          Enabled;
    BOOLEAN          BSP;
    VOLATILE BOOLEAN Online;
    VOID            *Stack;
} CPU_INFO;

BOOLEAN   SMPInit(LINEOS_BOOT_INFO *BootInfo);
UINT32    SMPGetCPUCount(VOID);
CPU_INFO *SMPGetCPU(UINT32 CPUID);
CPU_INFO *SMPGetCPUByAPICId(UINT32 APICID);
UINT32    SMPGetCurrentCPUID(VOID);
VOID      APMain(UINT32 CPUID);
