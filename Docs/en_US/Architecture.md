# Architecture

[Documentation index](README.md) · [한국어](../ko_KR/Architecture.md) · [English](../en_US/Architecture.md)

LineOS is a monolithic x86_64 kernel booted through UEFI. The bootloader loads the ELF64 kernel and supplies `LINEOS_BOOT_INFO`, including the memory map and ACPI RSDP. There is no legacy BIOS boot path.

## Boot and kernel initialization

`Kernel/Kernel.c` currently initializes physical and virtual memory, the heap, TrueType rendering, PCI and VirtIO GPU, the GDT and IDT, LAPIC/HPET/TSC timers, the UTC clock, SMP, LBPWRR, and VirtIO block/network/input devices.

`KMain()` currently draws saved frame values, registers the network, NTP and clock-display workers, and starts LBPWRR. It does not launch the former development shell, filesystem test task, or web viewer. Device initialization alone does not imply a task or user interface is running.

Application processors enter `APMain()`, initialize their CPU-local facilities, report themselves online, and join the scheduler.

## Source map

| Location | Responsibility |
| --- | --- |
| `BootLoader/` | UEFI entry, ELF loading and boot handoff |
| `Common/` | Shared definitions and boot structures |
| `Kernel/Kernel.c` | BSP/AP entry and current startup workload |
| `Kernel/Source/Arch/AMD64/` | CPU and synchronization primitives |
| `Kernel/Source/ACPI/`, `Multicore/` | Firmware tables and processor startup |
| `Kernel/Source/Interrupt/`, `Timer/` | Interrupt dispatch, APIC and clocks |
| `Kernel/Source/Memory/` | Physical/virtual memory and heap |
| `Kernel/Source/Scheduler/` | Tasks and LBPWRR scheduling |
| `Kernel/Source/PCI/`, `VirtIO/` | Shared device transport and queues |
| `Kernel/Source/Disk/`, `FS/` | Block devices, GPT, EXT2 and VFS |
| `Kernel/Source/Network/` | VirtIO-Net and network protocols |
| `Kernel/Source/Render/`, `Input/` | GPU, fonts and input devices |
| `Kernel/Include/` | Kernel interfaces |
| `Link/` | Kernel linker configuration |

## Boot and Architecture

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

## Multicore / SMP

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

After reporting themselves online, Application Processors enter `APMain()` and join the LBPWRR scheduler. Each CPU
begins with its own idle task and can run work assigned to its local run queue.

## Interrupts and Timers

- Interrupt Descriptor Table
- x86_64 exception handling
- Kernel panic infrastructure
- Local APIC initialization
- LAPIC End Of Interrupt handling
- HPET support
- TSC calibration
- TSC Deadline Timer
- Kernel timer interrupt infrastructure

See [clock and NTP](Clock.md) for wall time, uptime and synchronization.
