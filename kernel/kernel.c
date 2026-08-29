// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/truetype/truetype_engine.h>
#include <arch/x86_64/cpu.h>
#include <debug/panic.h>
#include <interrupt/apic.h>
#include <interrupt/idt.h>
#include <acpi/acpi.h>
#include <lineos/bootinfo.h>
#include <memory/memory.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <timer/hpet.h>
#include <timer/tsc.h>

#define BENCHMARK_FRAME_COUNT       1000ULL
#define BENCHMARK_DIRTY_FRAME_COUNT 3000ULL
#define BENCHMARK_DIRTY_RECTS       64U
#define BENCHMARK_DIRTY_RECT_SIZE   48U

typedef struct
{
    UINT64 Frames;
    UINT64 TotalTicks;
    UINT64 AverageTicks;
    UINT64 FPS;
} GPU_BENCHMARK_RESULT;

STATIC UINT32 BenchmarkColor(UINT64 Frame)
{
    if ((Frame & 1ULL) == 0)
    {
        return 0x111827FF;
    }

    return 0x1F2937FF;
}

STATIC GPU_BENCHMARK_RESULT MakeBenchmarkResult(UINT64 Frames, UINT64 Delta)
{
    GPU_BENCHMARK_RESULT Result;

    if (Delta == 0)
    {
        Panic(L"Benchmark delta is zero");
    }

    Result.Frames = Frames;
    Result.TotalTicks = Delta;
    Result.AverageTicks = Delta / Frames;
    Result.FPS = (UINT64) (((UINT128) TSCGetFrequency() * Frames) / Delta);

    return Result;
}

STATIC GPU_BENCHMARK_RESULT BenchmarkFillOnly(UINT64 Frames)
{
    UINT64 Start;
    UINT64 End;

    VirtIOGPUClearDirty();
    Start = RDTSC();

    for (UINT64 Frame = 0; Frame < Frames; Frame++)
    {
        FillScreen(BenchmarkColor(Frame));
        VirtIOGPUClearDirty();
    }

    End = RDTSC();
    return MakeBenchmarkResult(Frames, End - Start);
}

STATIC GPU_BENCHMARK_RESULT BenchmarkFullPresentOnly(UINT64 Frames)
{
    UINT64 Start;
    UINT64 End;
    UINT32 Width = VirtIOGPUGetFrameBufferWidth();
    UINT32 Height = VirtIOGPUGetFrameBufferHeight();

    Start = RDTSC();

    for (UINT64 Frame = 0; Frame < Frames; Frame++)
    {
        VirtIOGPUTransferRect(0, 0, Width, Height);
        VirtIOGPUFlushRect(0, 0, Width, Height);
    }

    End = RDTSC();
    return MakeBenchmarkResult(Frames, End - Start);
}

STATIC GPU_BENCHMARK_RESULT BenchmarkFullFrame(UINT64 Frames)
{
    UINT64 Start;
    UINT64 End;

    Start = RDTSC();

    for (UINT64 Frame = 0; Frame < Frames; Frame++)
    {
        FillScreen(BenchmarkColor(Frame));
        VirtIOGPUPresent();
    }

    End = RDTSC();
    return MakeBenchmarkResult(Frames, End - Start);
}

STATIC GPU_BENCHMARK_RESULT BenchmarkDirtyRects(UINT64 Frames)
{
    UINT64 Start;
    UINT64 End;
    UINT32 Width = VirtIOGPUGetFrameBufferWidth();
    UINT32 Height = VirtIOGPUGetFrameBufferHeight();
    UINT32 LimitX = Width > BENCHMARK_DIRTY_RECT_SIZE ? Width - BENCHMARK_DIRTY_RECT_SIZE : 1;
    UINT32 LimitY = Height > BENCHMARK_DIRTY_RECT_SIZE ? Height - BENCHMARK_DIRTY_RECT_SIZE : 1;

    Start = RDTSC();

    for (UINT64 Frame = 0; Frame < Frames; Frame++)
    {
        for (UINT32 Index = 0; Index < BENCHMARK_DIRTY_RECTS; Index++)
        {
            UINT32 X = (UINT32) ((Frame * 17ULL + Index * 97ULL) % LimitX);
            UINT32 Y = (UINT32) ((Frame * 31ULL + Index * 53ULL) % LimitY);

            FillRect(X, Y, BENCHMARK_DIRTY_RECT_SIZE, BENCHMARK_DIRTY_RECT_SIZE, BenchmarkColor(Frame + Index));
        }

        VirtIOGPUPresent();
    }

    End = RDTSC();
    return MakeBenchmarkResult(Frames, End - Start);
}

STATIC VOID PrintBenchmarkLine(CONST CHAR16 *Name, UINT32 Baseline, GPU_BENCHMARK_RESULT Result)
{
    KPrint(L"%s  fps=%llu  avg=%llu  ticks=%llu", 64, Baseline, 0xFFFFFFFF, 22, JETBRAINS_MONO, Name, Result.FPS, Result.AverageTicks, Result.TotalTicks);
}

STATIC VOID RunVirtIOGPUBenchmark(VOID)
{
    GPU_BENCHMARK_RESULT FillOnly;
    GPU_BENCHMARK_RESULT FullPresentOnly;
    GPU_BENCHMARK_RESULT FullFrame;
    GPU_BENCHMARK_RESULT DirtyRects;
    UINT32               Width = VirtIOGPUGetFrameBufferWidth();
    UINT32               Height = VirtIOGPUGetFrameBufferHeight();

    FillScreen(0x111827FF);
    VirtIOGPUPresent();

    FillOnly = BenchmarkFillOnly(BENCHMARK_FRAME_COUNT);
    FullPresentOnly = BenchmarkFullPresentOnly(BENCHMARK_FRAME_COUNT);
    FullFrame = BenchmarkFullFrame(BENCHMARK_FRAME_COUNT);
    DirtyRects = BenchmarkDirtyRects(BENCHMARK_DIRTY_FRAME_COUNT);

    FillScreen(0x111827FF);
    KPrint(L"VirtIO GPU benchmark", 64, 64, 0x22D3EEFF, 34, PRETENDARD);
    KPrint(L"resolution=%ux%u  tsc=%lluHz", 64, 112, 0xAAB4C8FF, 22, JETBRAINS_MONO, Width, Height, TSCGetFrequency());
    KPrint(L"dirty rects=%u x %ux%u", 64, 144, 0xAAB4C8FF, 22, JETBRAINS_MONO, BENCHMARK_DIRTY_RECTS, BENCHMARK_DIRTY_RECT_SIZE, BENCHMARK_DIRTY_RECT_SIZE);

    PrintBenchmarkLine(L"fill only         ", 208, FillOnly);
    PrintBenchmarkLine(L"present full only ", 248, FullPresentOnly);
    PrintBenchmarkLine(L"full frame        ", 288, FullFrame);
    PrintBenchmarkLine(L"dirty rect list   ", 328, DirtyRects);

    KPrint(L"full frame = FillScreen + full present", 64, 392, 0x677089FF, 20, PRETENDARD);
    KPrint(L"dirty rect list = 64 moving rectangles + rect present", 64, 424, 0x677089FF, 20, PRETENDARD);
    VirtIOGPUPresent();
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();

    KMemoryInit(BootInfo);
    TrueTypeInit();

    PCIInit(BootInfo);
    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    FillScreen(0x1E1E1E);
    VirtIOGPUFlush();

    IDTInit();
    IDTLoad();

    LAPICInit();

    if (!HPETInit(BootInfo))
    {
        Panic(L"HPET initialization failed");
    }

    if (!TSCCalibrate())
    {
        Panic(L"TSC calibration failed");
    }

    TSCDeadlineInit(0x40);

    STI();

    RunVirtIOGPUBenchmark();

    HLT();
}
