[![Arch](https://img.shields.io/badge/Arch-x86__64-24292f?style=flat-square)](https://github.com/kljj04/LineOS)
[![Language](https://img.shields.io/badge/Language-C-3465a4?style=flat-square)](https://github.com/kljj04/LineOS)
[![Boot](https://img.shields.io/badge/Boot-UEFI-555555?style=flat-square)](https://github.com/kljj04/LineOS)
[![Kernel](https://img.shields.io/badge/Kernel-Monolithic-24292f?style=flat-square)](https://github.com/kljj04/LineOS)
[![Platform](https://img.shields.io/badge/Platform-Bare_Metal-555555?style=flat-square)](https://github.com/kljj04/LineOS)

![Last Commit](https://img.shields.io/github/last-commit/kljj04/LineOS?style=flat-square)
![Repo Size](https://img.shields.io/github/repo-size/kljj04/LineOS?style=flat-square)
![Commits](https://img.shields.io/github/commit-activity/t/kljj04/LineOS?style=flat-square)

# LineOS

LineOS is a 64-bit x86_64 UEFI hobby operating system written primarily in freestanding C and x86 assembly.

The project currently focuses on multicore kernel bring-up, memory management, interrupt and timer infrastructure,
VirtIO devices, native TrueType rendering, kernel scheduling, and an EXT2 filesystem integrated with a virtual
filesystem layer (VFS) and an interactive kernel shell.

## Status

LineOS is experimental and under active development.

Internal APIs, scheduler design, memory management, device drivers, boot structures, and build scripts may change
frequently.

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
11. The LBPWRR scheduler, VirtIO block device, and VirtIO input devices are initialized.
12. Keyboard, mouse, input-monitor, and disk/filesystem test tasks are created and registered.
13. `LBPWRRStart()` begins scheduled execution; the disk task mounts GPT partition 2 and runs EXT2 tests.

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

After reporting themselves online, Application Processors enter `APMain()` and join the LBPWRR scheduler. Each CPU
begins with its own idle task and can run work assigned to its local run queue.

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

`LBPWRRStart()` selects a local runnable task and restores its saved interrupt frame. It arms a new TSC deadline
immediately before the first context restore so a BSP that reached the scheduler with interrupts disabled still receives
its first timer interrupt after task entry.

The timer interrupt saves the current task frame, updates task state, selects the next local task from the weighted
chunk, and returns that task's saved frame to the common ISR restore path. Non-idle tasks are preferred; the local idle
task is used only when no non-idle task is ready.

The scheduler is experimental. CPU affinity, blocking beyond timed sleep, fairness measurement, and broader SMP
subsystem hardening remain future work.

### PCI and VirtIO

- PCI configuration space scanning
- Q35 machine support
- Generic VirtIO PCI transport infrastructure
- Generic VirtQueue infrastructure
- VirtIO GPU
- VirtIO keyboard input
- VirtIO tablet input
- VirtIO block device with synchronous and asynchronous reads, writes, and flushes

### Storage and EXT2

- GPT partition lookup with header and partition-entry-array CRC validation
- Mounting the second GPT partition as a Linux EXT2 filesystem
- 1, 2, and 4 KiB blocks; revision 0/1; tested 128/256-byte inodes
- Superblock, block-group descriptor, block/inode bitmap, and inode handling
- Absolute path lookup, including relative and absolute symbolic links
- File and directory creation; file reads, writes, sparse extension, and truncation
- Direct, single-, double-, and triple-indirect block addressing
- File removal, empty-directory removal, and block/inode reclamation
- Same-volume rename, target replacement, and cross-directory moves
- Hard links and inline or block-backed symbolic links
- 32-bit UID/GID ownership and basic owner/group/other permission checks
- Sticky-directory checks and setgid directory inheritance
- Writable-mount validation of block ownership, allocation counts, directory references, and parent relationships
- Bounded in-memory preparation followed by dirty-state and flush-ordered disk updates

EXT2 is connected to a VFS core with per-partition roots, path resolution, directory enumeration, and command
contexts. `/` means the selected partition root; `^` is an administrator-only virtual partition manager. Normal users
receive a not-found result for that namespace. Administrator privileges do not add a prefix to ordinary `/A/B` paths.
There is no process file-descriptor table, open-file lifetime layer, or user-mode authentication system yet.

A single transaction can stage at most 32768 changed blocks (about 32–128 MiB including metadata). Oversized operations
fail before committing. Active
extended attributes/ACLs, supplementary groups, journaling, and unsupported features or inode flags are not supported
for modification. EXT2 has no journal here: dirty marking and flush ordering do not guarantee atomic recovery or data
preservation after real power loss. A failed disk commit blocks further access through that mount and requires offline
filesystem checking before the next writable mount.

The public filesystem entry points are declared in `Kernel/Include/FS/EXT2/EXT2.h`.

### UTC Clock and Development Shell

- RTC-backed UTC Unix time through `ClockGetUnixTime()`; local display offsets do not affect disk timestamps
- EXT2 ctime for inode changes, mtime for content changes, and relatime atime updates on writable mounts
- A basic kernel development shell in `Kernel.c`, with real VFS-backed file commands
- `pwd`, `cd`, `ls`, `stat`, `cat`, `hexdump`, `mkdir`, `touch`, `write`, `append`, `cp`, `mv`, `rm`, `rmdir`, `ln`,
  `truncate`, `chmod`, `chown`, `echo`, `time`, `uptime`, `whoami`, `sync`, `clear`, and `history`
- One-command debug `sudo`; successful `sudo cd` preserves location without preserving credentials
- US ASCII keyboard input, quoting/escaping, Backspace, and recent-command recall

The local debug console has no password or login authentication. It starts as UID/GID 1000 and grants command-local
sudo authorization to the console operator. This is a development policy, not a completed userland security system.
EFI p1 is a reserved partition entry; its FAT32 backend remains unimplemented.

See [VFS paths, shell commands, timestamp policy, limits, and test details](Docs/VFS.md).

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
./Run.sh
```

The launcher uses `sudo` for loop-device attachment, partition mounting, and copying the EFI boot files into the raw
disk image. An interactive `sudo` password or equivalent local permission is required.

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

Runtime logs are written to `Logs/QEMU.log` and `Logs/DebugCon.log`. The latter contains EXT2 test progress,
`[EXT2 TEST] PASS` or a failure stage/error code, and debug output from panic and exception paths.

## Font Rendering

LineOS currently embeds two TrueType font files into the kernel image using assembler `.incbin`:

- `Kernel`: Pretendard SemiBold
- `Kernel`: JetBrains Mono Nerd Font SemiBold

The kernel performs runtime TrueType glyph rasterization and draws the resulting glyphs into the VirtIO GPU framebuffer.

Current rendering support includes:

- English ASCII
- Digits and symbols
- Korean Hangul
- Nerd Font private-use glyphs

The bundled fonts are third-party assets. See `Docs/THIRD_PARTY_NOTICES.md` and `Docs/LICENSES`.

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

LBPWRR uses a fixed-capacity per-CPU runnable list and a weighted chunk. A task's priority maps directly to a weight
from 1 through 6; each task appears in the chunk according to that weight. Adding a task selects the CPU with the
smallest runnable count, then updates that CPU's chunk while holding its run-queue lock.

Task state changes update the corresponding weighted chunk. Sleeping, terminated, and killed tasks do not remain
runnable. A task must be removed from scheduler ownership before its task storage and stack are destroyed.

Migration is intentionally limited to scheduled `TASK_READY` tasks that are not the current task on their source CPU. It
acquires the two run-queue locks in CPU-index order and updates both the runnable lists and weighted chunks atomically
with respect to those locks.

## Current Kernel Test Workload

The current `Kernel/Kernel.c` runs keyboard and mouse handlers, an on-screen input monitor, and a VirtIO block/EXT2
test task. The clock display also runs continuously. After filesystem tests pass, a development shell mounts p2 for
interactive
file operations. The former scheduler stress workload is not the default kernel entry workload.

The disk task verifies GPT and EXT2 recognition, then mounts partition 2 writable. It chooses an unused
`/LineOSTestNNNN` directory and retains `Payload.bin` for inspection. Each successful run leaves a new test directory;
existing test files are not overwritten. The task performs real writes to the root partition used by the VM.

The filesystem checks cover direct/single-indirect data, double-indirect sparse writes and zero-filled holes, EOF and
invalid-operation rejection, truncation and regrowth, hard and symbolic links, rename, deletion, ownership and
permissions, and persistence after a read-only remount. Temporary files used by the additional checks are removed.
Results appear on screen and through the debug console.

## EXT2 Verification

The 2026-10-19 checkpoint passed:

- 182 host image scenarios using the actual kernel EXT2/GPT sources with AddressSanitizer, UndefinedBehaviorSanitizer,
  and compiler warnings treated as errors
- Block/inode-size combinations, revision 0, triple-indirect sparse files, allocation/reclamation, path/link handling,
  permission checks, and remount persistence
- Rejection of tested corrupt metadata, unsupported features, dirty writable mounts, and invalid GPT CRCs
- Allocation, disk-write, and flush failures, plus process termination at disk-write/flush boundaries; interrupted
  images were repaired with `e2fsck` when needed and then checked again
- Actual QEMU/KVM kernel tests with both 1 and 16 virtual CPUs reaching `[EXT2 TEST] PASS`
- `e2fsck -fn` checks of both resulting QEMU EXT2 partitions without errors

The QEMU checks used separate test disks and copied firmware variables. Host process termination models an
interruption in a file-backed device; it does not reproduce every real device-cache or power-loss behavior.

The 182-scenario results above are the earlier EXT2 checkpoint. The current Clock/VFS/shell regression suite is:

```bash
python3 Tests/FS/Run.py --work /tmp/LineOSFSTests
```

The current checkpoint passes 13 clock checks, 144 filesystem/VFS checks for each of 1/2/4 KiB blocks, and 117 shell
checks under ASan/UBSan. QEMU/KVM with 1 and 16 CPUs also passed the kernel tests and 27 actual keyboard-driven shell
commands each; resulting disks passed `e2fsck`. See [the detailed contract and limitations](Docs/VFS.md).

## Planned Work

Near-term:

- SMP synchronization cleanup
- SMP-safe VirtIO and kernel subsystems
- Scheduler fairness and stress regression coverage
- CPU affinity and broader task blocking support
- EXT2 hardening and broader device/failure regression coverage
- Kernel console

Later:

- Open-file lifetime management, process file descriptors, and userland shell/authentication integration
- Virtual memory manager improvements
- User mode
- System calls
- Executable loading
- Process isolation

## License

LineOS is licensed under the POL License. See `LICENSE`.

Third-party fonts are licensed separately under the SIL Open Font License 1.1 and BSD-2-Clause-Patent. See
`Docs/THIRD_PARTY_NOTICES.md`.
