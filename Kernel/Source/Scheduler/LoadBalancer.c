// Kernel/Source/Scheduler/LoadBalancer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Scheduler/LoadBalancer.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/LBPWRRRunQueue.h>
#include <Scheduler/Usage.h>

VOID LoadBalancerBalance(VOID)
{
    UINT32 CPUCount    = LBPWRRGetActiveCPUCount();
    UINT32 TargetCPUID = 0;
    UINTN  TargetUsage = 100;

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        UINTN Usage = UsageGetCPU(CPUID);

        if (Usage < TargetUsage)
        {
            TargetCPUID = CPUID;
            TargetUsage = Usage;
        }
    }

    UINT32 SourceCPUID = CPUCount;
    UINTN  SourceUsage = TargetUsage;

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        UINTN Usage = UsageGetCPU(CPUID);

        if (Usage >= TargetUsage + 15 && Usage > SourceUsage && LBPWRRGetCPUAssignedTaskCount(CPUID) > 1)
        {
            SourceCPUID = CPUID;
            SourceUsage = Usage;
        }
    }

    if (SourceCPUID < CPUCount)
    {
        LBPWRRMoveTask(SourceCPUID, TargetCPUID, SourceUsage - TargetUsage);
    }
}
