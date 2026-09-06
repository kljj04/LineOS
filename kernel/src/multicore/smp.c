// kernel/src/multicore/smp.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <acpi/acpi.h>
#include <arch/x86_64/cpu.h>
#include <interrupt/apic.h>
#include <memory/memory.h>
#include <multicore/smp.h>
#include <timer/tsc.h>

#define ACPI_MADT_TYPE_LOCAL_APIC   0
#define ACPI_MADT_TYPE_LOCAL_X2APIC 9

#define ACPI_MADT_ENABLED        (1U << 0)
#define ACPI_MADT_ONLINE_CAPABLE (1U << 1)

#define SMP_AP_START_TIMEOUT_MS 100

#define SMP_PAGE_SIZE 4096ULL

EXTERN UINT8 APTrampolineStart[];
EXTERN UINT8 APTrampolineEnd[];

STATIC CPU_INFO CPUs[SMP_MAX_CPUS];
STATIC UINT32 CPUCount = 0;

STATIC UINT64 SMPReadCR3(VOID)
{
    UINT64 CR3;

    ASM("mov %%cr3, %0" : "=r"(CR3));

    return CR3;
}

STATIC BOOLEAN SMPAddCPU(UINT32 APICID, BOOLEAN enabled)
{
    CPU_INFO *CPU;

    if (!enabled || CPUCount >= SMP_MAX_CPUS)
    {
        return FALSE;
    }

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        if (CPUs[CPUID].APICID == APICID)
        {
            return TRUE;
        }
    }

    CPU = &CPUs[CPUCount];

    CPU->CPUID = CPUCount;
    CPU->APICID = APICID;
    CPU->Enabled = TRUE;
    CPU->BSP = FALSE;
    CPU->Online = FALSE;
    CPU->Stack = NULL;

    CPUCount++;

    return TRUE;
}

STATIC BOOLEAN SMPDetectBSP(VOID)
{
    UINT32 BSPAPICID;

    BSPAPICID = LAPICGetID();

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        if (CPUs[CPUID].APICID != BSPAPICID)
        {
            continue;
        }

        CPUs[CPUID].BSP = TRUE;
        CPUs[CPUID].Online = TRUE;

        return TRUE;
    }

    return FALSE;
}

STATIC BOOLEAN SMPPrepareTrampoline(VOID)
{
    UINT64 CR3;
    UINTN TrampolineSize;

    CR3 = SMPReadCR3();

    if ((CR3 >> 32) != 0)
    {
        return FALSE;
    }

    TrampolineSize = (UINTN) (APTrampolineEnd - APTrampolineStart);

    if (TrampolineSize == 0 || TrampolineSize > 0xF00)
    {
        return FALSE;
    }

    KMemSet((VOID *) SMP_TRAMPOLINE_ADDRESS, 0, SMP_PAGE_SIZE);

    KMemCpy((VOID *) SMP_TRAMPOLINE_ADDRESS, APTrampolineStart, TrampolineSize);

    *(VOLATILE UINT32 *) SMP_TRAMPOLINE_CR3_ADDRESS = (UINT32) CR3;

    return TRUE;
}

STATIC BOOLEAN SMPStartAP(CPU_INFO *CPU)
{
    UINT64 StackTop;
    UINT8 vector;

    if (CPU == NULL || CPU->BSP || !CPU->Enabled)
    {
        return FALSE;
    }

    if (CPU->APICID > 0xFF)
    {
        return FALSE;
    }

    CPU->Stack = KAllocPages(SMP_AP_STACK_PAGES);

    if (CPU->Stack == NULL)
    {
        return FALSE;
    }

    StackTop = (UINT64) CPU->Stack + (SMP_AP_STACK_PAGES * SMP_PAGE_SIZE);

    *(VOLATILE UINT64 *) SMP_TRAMPOLINE_STACK_ADDRESS = StackTop;
    *(VOLATILE UINT64 *) SMP_TRAMPOLINE_ENTRY_ADDRESS = (UINT64) APMain;
    *(VOLATILE UINT32 *) SMP_TRAMPOLINE_CPUID_ADDRESS = CPU->CPUID;
    CompilerBarrier();

    CPU->Online = FALSE;
    CompilerBarrier();

    vector = (UINT8) (SMP_TRAMPOLINE_ADDRESS >> 12);

    if (!LAPICSendINIT(CPU->APICID))
    {
        return FALSE;
    }

    SleepMls(10);

    if (!LAPICSendSIPI(CPU->APICID, vector))
    {
        return FALSE;
    }

    SleepMls(1);

    if (!LAPICSendSIPI(CPU->APICID, vector))
    {
        return FALSE;
    }

    for (UINT32 elapsed = 0; elapsed < SMP_AP_START_TIMEOUT_MS; elapsed++)
    {
        CompilerBarrier();
        if (CPU->Online)
        {
            CompilerBarrier();
            return TRUE;
        }

        SleepMls(1);
    }

    return FALSE;
}

BOOLEAN SMPInit(LINEOS_BOOT_INFO *BootInfo)
{
    ACPI_MADT *MADT;
    UINT8 *entry;
    UINT8 *end;
    BOOLEAN HasAP;

    CPUCount = 0;

    MADT = (ACPI_MADT *) ACPIFindTable(BootInfo, (CONST CHAR8 *) "APIC");

    if (MADT == NULL || MADT->Header.Length < sizeof(ACPI_MADT))
    {
        return FALSE;
    }

    entry = MADT->Entries;
    end = (UINT8 *) MADT + MADT->Header.Length;

    while (entry + sizeof(ACPI_MADT_ENTRY) <= end)
    {
        ACPI_MADT_ENTRY *Entry;

        Entry = (ACPI_MADT_ENTRY *) entry;

        if (Entry->Length < sizeof(ACPI_MADT_ENTRY))
        {
            return FALSE;
        }

        if (entry + Entry->Length > end)
        {
            return FALSE;
        }

        if (Entry->Type == ACPI_MADT_TYPE_LOCAL_APIC && Entry->Length >= sizeof(ACPI_MADT_LOCAL_APIC))
        {
            ACPI_MADT_LOCAL_APIC *LocalAPIC;

            LocalAPIC = (ACPI_MADT_LOCAL_APIC *) Entry;

            SMPAddCPU(LocalAPIC->APICID, (LocalAPIC->Flags & (ACPI_MADT_ENABLED | ACPI_MADT_ONLINE_CAPABLE)) != 0);
        }
        else if (Entry->Type == ACPI_MADT_TYPE_LOCAL_X2APIC && Entry->Length >= sizeof(ACPI_MADT_LOCAL_X2APIC))
        {
            ACPI_MADT_LOCAL_X2APIC *LocalX2APIC;

            LocalX2APIC = (ACPI_MADT_LOCAL_X2APIC *) Entry;

            SMPAddCPU(LocalX2APIC->X2APICID, (LocalX2APIC->Flags & (ACPI_MADT_ENABLED | ACPI_MADT_ONLINE_CAPABLE)) != 0);
        }

        entry += Entry->Length;
    }

    if (CPUCount == 0)
    {
        return FALSE;
    }

    if (!SMPDetectBSP())
    {
        return FALSE;
    }

    HasAP = FALSE;

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        if (!CPUs[CPUID].BSP)
        {
            HasAP = TRUE;
            break;
        }
    }

    if (!HasAP)
    {
        return TRUE;
    }

    if (!SMPPrepareTrampoline())
    {
        return FALSE;
    }

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        CPU_INFO *CPU = &CPUs[CPUID];

        if (CPU->BSP)
        {
            continue;
        }

        if (!SMPStartAP(CPU))
        {
            return FALSE;
        }
    }

    return TRUE;
}

UINT32 SMPGetCPUCount(VOID)
{
    return CPUCount;
}

CPU_INFO *SMPGetCPU(UINT32 CPUID)
{
    if (CPUID >= CPUCount)
    {
        return NULL;
    }

    return &CPUs[CPUID];
}

CPU_INFO *SMPGetCPUByAPICId(UINT32 APICID)
{
    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        if (CPUs[CPUID].APICID == APICID)
        {
            return &CPUs[CPUID];
        }
    }

    return NULL;
}

UINT32 SMPGetCurrentCPUID(VOID)
{
    UINT32 APICID;

    APICID = LAPICGetID();

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        if (CPUs[CPUID].APICID == APICID)
        {
            return CPUs[CPUID].CPUID;
        }
    }

    return UINT32_MAX;
}