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

Current SMP support is bring-up only. Application Processors currently enter `APMain()` and remain idle after reporting themselves online.

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

- Primary Round Robin (PRR) scheduler
- Kernel task creation
- Timer-driven scheduling
- Explicit task yield support

The current PRR scheduler predates full SMP scheduling support.

The planned scheduler architecture is **LBPWRR (Load-Balanced Primary Weighted Round Robin)**, with SMP-aware scheduling and load balancing across online processors.

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

Runtime logs are written under `logs/`.

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

The bundled fonts are third-party assets. See `THIRD_PARTY_NOTICES.md` and `LICENSES/OFL-1.1.txt`.

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

## Scheduler Roadmap

The existing scheduler is **PRR (Primary Round Robin)**.

PRR provides the initial kernel task and context-switching infrastructure, but it was designed before SMP bring-up.

The next scheduler is planned as:

**LBPWRR — Load-Balanced Primary Weighted Round Robin**

The goal is to extend scheduling across the processors brought online by the SMP subsystem.

Planned work includes:

- Per-CPU scheduler state
- SMP-aware run queues
- Weighted task scheduling
- Load balancing
- Task migration
- CPU affinity infrastructure
- SMP-aware timer scheduling
- Idle tasks for APs
- Scheduler synchronization

## Planned Work

Near-term:

- LBPWRR multicore scheduler
- Per-CPU scheduler infrastructure
- Load balancing across online CPUs
- SMP synchronization cleanup
- SMP-safe VirtIO and kernel subsystems
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