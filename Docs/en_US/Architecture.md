# Architecture

[Documentation index](README.md) · [한국어](../ko_KR/Architecture.md) · [English](../en_US/Architecture.md)

LineOS is a monolithic x86_64 kernel booted through UEFI. The bootloader loads the ELF64 kernel and supplies `LINEOS_BOOT_INFO`, including the memory map and ACPI RSDP. There is no legacy BIOS boot path.

## Boot and kernel initialization

`Kernel/Kernel.c` currently initializes physical and virtual memory, the heap, TrueType rendering, PCI and VirtIO GPU, the GDT and IDT, LAPIC/HPET/TSC timers, the UTC clock, SMP, LBPWRR, and VirtIO block/network/input devices.

`InitKernel()` initializes VFS, mounts GPT partition 2 as an EXT2 root and initializes network and input devices. `KMain()` loads `/bin/Init.lix` as a static ELF64 userspace task, panics with `Init Failed` if loading fails, and starts LBPWRR. Init publishes fonts as shared assets before calling `spawn("/bin/Shell.lix")`. Shell maps the font read-only and displays the shared output stream. See [user programs](UserLibraries.md) for details and current limits.

The keyboard and mouse consumers, NetworkTask and ClockNTPTask are not registered by this startup path. Device initialization alone does not start these workers. See [scheduling](Scheduler.md) for Wait Queues and CPU accounting.

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
| `UserPrograms/`, `LOSLibC/`, `Libraries/` | Userspace programs and static libraries |
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

## Host configuration

The launcher reads `Configs/QEMUSettings.sh`. Build flags and target discovery are in `Makefiles/BuildSettings.mk`; the Bash backend and Node.js interface are under `Scripts/Backend/` and `Scripts/Frontend/`. See [building and running](Build.md) for all current configuration values.
