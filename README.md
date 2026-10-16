# LineOS

LineOS is a 64-bit x86_64 UEFI hobby operating system written primarily in freestanding C and x86 assembly.

The project currently focuses on multicore kernel bring-up, memory management, interrupt and timer infrastructure, VirtIO devices, native TrueType rendering, and kernel scheduling.

## Status

LineOS is experimental and under active development.

Internal APIs, scheduler design, memory management, device drivers, boot structures, and build scripts may change frequently.

Current kernel bring-up:

1. UEFI starts `BOOTX64.EFI`.
2. The bootloader collects the UEFI memory map, ACPI RSDP, and kernel metadata.
3. The bootloader loads `LINEOS_KERNEL.ELF`.
4. Boot information is passed through `LINEOS_BOOT_INFO`.
5. The kernel initializes the Buddy PMM and kernel heap.
6. PCI and VirtIO GPU are initialized.
7. The IDT, LAPIC, HPET, and TSC Deadline Timer are initialized.
8. ACPI MADT is used to enumerate processors.
9. Application Processors are started through INIT-SIPI-SIPI and an x86 trampoline.
10. APs enter 64-bit Long Mode and call `APMain()`.
11. VirtIO input and the kernel scheduler are initialized.

SMP bring-up has been tested with 16 virtual CPUs under QEMU.

## Current Features

### Boot and Architecture

- Custom x86_64 UEFI bootloader
- ELF64 kernel loading
- `LINEOS_BOOT_INFO` boot handoff
- ACPI RSDP discovery
- ACPI table lookup
- MADT processor enumeration
- UEFI memory map handoff
- x86_64 Long Mode kernel
- Microsoft x64 ABI boot entry
- No legacy BIOS boot path

### Multicore / SMP

- BSP and AP detection
- Local APIC ID enumeration
- xAPIC INIT IPI support
- xAPIC Startup IPI support
- INIT-SIPI-SIPI AP startup sequence
- Low-memory AP startup trampoline
- 16-bit -> 32-bit -> 64-bit AP transition
- Shared BSP page tables during AP startup
- Per-AP kernel stacks
- AP online state tracking
- SMP bring-up tested with 16 QEMU vCPUs
- Maximum CPU table size currently set to 256 CPUs

After reporting themselves online, Application Processors enter `APMain()` and join the LBPWRR scheduler. Each CPU begins with its own idle task and can run work assigned to its local run queue.

### Memory Management

- Buddy physical memory allocator
- Power-of-two physical page allocation
- Physical allocations below an address limit
- Kernel page allocation and release
- Kernel heap allocator
- `KAlloc()` / `KFree()`
- Spinlock synchronization primitives
- Basic freestanding memory functions:
    - `KMemCpy`
    - `KMemMove`
    - `KMemSet`

### Interrupts and Timers

- Interrupt Descriptor Table
- x86_64 exception handling
- Kernel panic infrastructure
- Local APIC initialization
- LAPIC End Of Interrupt handling
- HPET support
- TSC calibration
- TSC Deadline Timer
- Kernel timer interrupt infrastructure

### Scheduling

- LBPWRR (Load-Balanced Primary Weighted Round Robin) scheduler
- Per-CPU run queues and current-task tracking
- Per-CPU idle tasks
- Weighted runnable chunks with priorities 1 through 6
- Least-loaded CPU selection when adding a task
- Timer-driven scheduling through the TSC Deadline Timer
- Explicit task yield support
- Sleeping task wake-up based on TSC deadlines
- Task termination, removal from the run queue, and destruction
- Ready-task migration with ordered dual-run-queue locking

`LBPWRRStart()` selects a local runnable task and restores its saved interrupt frame. It arms a new TSC deadline immediately before the first context restore so a BSP that reached the scheduler with interrupts disabled still receives its first timer interrupt after task entry.

The timer interrupt saves the current task frame, updates task state, selects the next local task from the weighted chunk, and returns that task's saved frame to the common ISR restore path. Non-idle tasks are preferred; the local idle task is used only when no non-idle task is ready.

The scheduler is experimental. CPU affinity, blocking beyond timed sleep, fairness measurement, and broader SMP subsystem hardening remain future work.

### PCI and VirtIO

- PCI configuration space scanning
- Q35 machine support
- Generic VirtIO PCI transport infrastructure
- Generic VirtQueue infrastructure
- VirtIO GPU
- VirtIO keyboard input
- VirtIO tablet input

### Graphics

- VirtIO GPU initialization
- VirtIO GPU framebuffer creation
- Scanout setup
- Framebuffer transfer and flushing
- Dirty rectangle tracking
- Hardware cursor support
- Pixel read/write
- Rectangle drawing
- Line drawing
- Circle drawing
- Alpha blending
- Framebuffer copy operations

### Text Rendering

- Runtime TrueType parsing and rasterization
- Embedded fonts through assembler `.incbin`
- Pretendard SemiBold
- JetBrains Mono Nerd Font SemiBold
- ASCII rendering
- Korean Hangul rendering
- Nerd Font private-use glyph rendering
- Runtime glyph blending into the VirtIO GPU framebuffer

## Architecture

- CPU architecture: `x86_64`
- Firmware: `UEFI`
- Kernel format: `ELF64`
- Bootloader ABI: Microsoft x64 ABI
- Kernel language: freestanding C and x86 assembly
- Main emulator: QEMU
- Machine type: `q35`
- Graphics device: `virtio-gpu-pci`
- Input devices:
    - `virtio-keyboard-pci`
    - `virtio-tablet-pci`
- Storage device: `virtio-blk-pci`
- Boot disk format: raw GPT image
- Root filesystem format: EXT2
- BIOS boot: not supported

## Build Requirements

Linux is the primary development environment.

Typical requirements:

- `clang`
- `ld.lld`
- `make`
- `qemu-system-x86_64`
- `qemu-img`
- `parted`
- `mkfs.vfat`
- `mkfs.ext2`
- `losetup`
- OVMF firmware

KVM acceleration is used by the default launcher.

## Running

From the project root:

```bash
./run.sh
```

The launcher uses `sudo` for loop-device attachment, partition mounting, and copying the EFI boot files into the raw disk image. An interactive `sudo` password or equivalent local permission is required.

The current Linux launcher:

1. Builds the bootloader and kernel.
2. Creates an 8 GiB raw disk image if necessary.
3. Creates a GPT partition table.
4. Creates a 300 MiB FAT32 EFI System Partition.
5. Creates an EXT2 LineOS root partition.
6. Copies `BOOTX64.EFI` and `LINEOS_KERNEL.ELF`.
7. Starts QEMU using Q35, OVMF, VirtIO GPU, VirtIO block, and VirtIO input devices.

The current default QEMU configuration uses:

- 4 GiB RAM
- 16 virtual CPUs
- KVM acceleration
- QHD (`2560x1440`) display
- VirtIO GPU
- VirtIO keyboard
- VirtIO tablet
- VirtIO block device

Runtime logs are written under `logs/`. `qemu.log` contains QEMU diagnostics, and `debugcon.log` is reserved for panic and exception output. The normal scheduler stress workload does not write progress messages to the debug console.

## Font Rendering

LineOS currently embeds two TrueType font files into the kernel image using assembler `.incbin`:

- `kernel/assets/fonts/PTDSB.ttf`: Pretendard SemiBold
- `kernel/assets/fonts/JBMNFSB.ttf`: JetBrains Mono Nerd Font SemiBold

The kernel performs runtime TrueType glyph rasterization and draws the resulting glyphs into the VirtIO GPU framebuffer.

Current rendering support includes:

- English ASCII
- Digits and symbols
- Korean Hangul
- Nerd Font private-use glyphs

The bundled fonts are third-party assets. See `THIRD_PARTY_NOTICES.md` and `LICENSES/OFL-1.1`.

## Memory Notes

Physical memory management uses a Buddy allocator initialized from the UEFI memory map.

The kernel also provides a heap allocator on top of the physical memory infrastructure.

Important boot metadata includes:

- UEFI memory map pointer and descriptor information
- ACPI RSDP pointer
- Kernel base address
- Kernel size
- Kernel entry address

The kernel image and other reserved regions must remain unavailable to the physical allocator.

## Scheduler Notes

LBPWRR uses a fixed-capacity per-CPU runnable list and a weighted chunk. A task's priority maps directly to a weight from 1 through 6; each task appears in the chunk according to that weight. Adding a task selects the CPU with the smallest runnable count, then updates that CPU's chunk while holding its run-queue lock.

Task state changes update the corresponding weighted chunk. Sleeping, terminated, and killed tasks do not remain runnable. A task must be removed from scheduler ownership before its task storage and stack are destroyed.

Migration is intentionally limited to scheduled `TASK_READY` tasks that are not the current task on their source CPU. It acquires the two run-queue locks in CPU-index order and updates both the runnable lists and weighted chunks atomically with respect to those locks.

## Scheduler Stress Workload

The default kernel entry starts a scheduler stress workload after kernel initialization. Its on-screen monitor reports the current phase, worker counters, sleep and yield activity, task churn progress, migration results, and a failure mask.

The workload exercises timer preemption, timed sleep and wake-up, explicit yield, creation and termination of 32 short-lived tasks, priority changes, ready-task migration, task killing, task termination, and the idle-only transition. It is a runtime regression workload rather than a proof of scheduler correctness on all hardware or CPU counts.

## Planned Work

Near-term:

- SMP synchronization cleanup
- SMP-safe VirtIO and kernel subsystems
- Scheduler fairness and stress regression coverage
- CPU affinity and broader task blocking support
- EXT2 filesystem implementation
- VirtIO block driver integration
- Kernel console

Later:

- Virtual memory manager improvements
- User mode
- System calls
- Executable loading
- Process isolation

## License

LineOS is licensed under the MIT License. See `LICENSE`.

Third-party fonts are licensed separately under the SIL Open Font License 1.1. See `THIRD_PARTY_NOTICES.md`.
