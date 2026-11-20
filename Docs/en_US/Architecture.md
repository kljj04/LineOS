# Architecture

[Documentation index](README.md) · [한국어](../ko_KR/Architecture.md) · [English](../en_US/Architecture.md)

LineOS is a monolithic x86_64 kernel booted through UEFI. The bootloader loads the ELF64 kernel and supplies `LINEOS_BOOT_INFO`, including the memory map and ACPI RSDP. Kernel entry uses the Microsoft x64 ABI; there is no legacy BIOS boot path.

## Boot and kernel initialization

`Kernel/Kernel.c` currently initializes physical and virtual memory, the heap, TrueType rendering, PCI and VirtIO GPU, the GDT and IDT, LAPIC/HPET/TSC timers, the UTC clock, SMP, LBPWRR, and VirtIO block/network/input devices.

`InitKernel()` initializes VFS and mounts GPT partition 2 as EXT2 root plus DeviceFS at /dev. `KMain()` starts keyboard/mouse consumers, loads /sbin/Init.lix as a musl dynamic PIE ELF64 userspace process and starts LBPWRR. Generated Program.cfg startup code publishes Init’s fonts; Init opens the terminal endpoints, and uses fork followed by child execve to launch /sbin/Terminal.lix and /sbin/Login.lix. Terminal maps /dev/fb0 and renders output read from descriptor 3. Login starts user autostart entries and /bin/lsh/lsh.lix after authentication. See [user programs](UserLibraries.md) for limits.

KMain registers keyboard/mouse consumers and ClockNTPTask, but not NetworkTask. An NTP worker therefore does not imply automatic network configuration or receive processing. See [scheduling](Scheduler.md) for Wait Queues and CPU accounting.

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
| `UserPrograms/`, `Libraries/LOSLib/`, `Libraries/` | Userspace programs, musl and LOS shared libraries |
| `Kernel/Include/` | Kernel interfaces |
| `Link/` | Kernel linker configuration |

## SMP

MADT enumeration identifies processors. INIT-SIPI-SIPI starts APs through a low-memory trampoline, transitioning from 16 to 32 to 64 bits. APs initially share BSP page tables and have separate kernel stacks. APMain initializes CPU-local facilities, reports online state and joins the scheduler. The CPU table currently holds up to 256 entries; each CPU has its own idle task and local run queue.

## Interrupts and timers

The kernel provides IDT exception/interrupt handling, panic output, LAPIC initialization/EOI, HPET, TSC calibration and deadline timers. See [Scheduler](Scheduler.md) for switching and [Clock](Clock.md) for wall time and NTP.

## Host configuration

The launcher reads `Configs/QEMUSettings.sh`. Build flags and target discovery are in `Makefiles/BuildSettings.mk`; the Bash backend and Node.js interface are under `Scripts/Backend/` and `Scripts/Frontend/`. See [building and running](Build.md) for all current configuration values.
