# LineOS

LineOS is a 64-bit UEFI hobby operating system written mostly in freestanding C.

The project is currently focused on bootstrapping a small graphical kernel, PCI discovery, VirtIO devices, physical memory management, and native Korean text rendering.

## Status

LineOS is experimental. Internal APIs, boot data, device drivers, memory layout, and build scripts can change quickly.

Current boot flow:

1. UEFI starts `BOOTX64.EFI`.
2. The bootloader initializes GOP, ACPI, and the memory map.
3. The bootloader loads `LINEOS_KERNEL.ELF`.
4. Boot metadata is passed through `LINEOS_BOOT_INFO`.
5. The kernel initializes memory, PCI, VirtIO GPU, and renders a graphical test screen.

## Current Features

- Custom UEFI bootloader
- ELF64 kernel loading
- Boot information handoff
- UEFI GOP discovery
- ACPI RSDP discovery
- UEFI memory map handoff
- Physical page bitmap allocator
- Basic memory functions: `KMemCpy`, `KMemMove`, `KMemSet`
- PCI configuration space scanning
- Q35 machine support
- VirtIO PCI transport helpers
- VirtIO GPU initialization
- VirtIO GPU framebuffer creation, transfer, scanout, and flush
- Raw disk image boot flow
- GPT disk image with EFI and EXT3 partitions
- Debug output through QEMU debugcon port `0xE9`
- TrueType font parsing and rasterization
- Pretendard SemiBold Korean rendering
- JetBrains Mono Nerd Font rendering
- ASCII, Hangul, and Nerd Font glyph test output

## Architecture

- CPU architecture: `x86_64`
- Firmware: `UEFI`
- Kernel format: `ELF64`
- Bootloader ABI: Microsoft x64 ABI
- Kernel language: freestanding C
- Main emulator target: QEMU
- Machine type: `q35`
- Graphics device: `virtio-gpu-pci`
- Storage device: `virtio-blk-pci`
- Boot disk format: raw image
- BIOS boot: not supported

## Repository Layout

- `bootloader/`: UEFI bootloader sources
- `common/`: shared boot structures and type definitions
- `kernel/`: kernel sources, drivers, memory, rendering, and assets
- `kernel/assets/fonts/`: bundled test fonts
- `link/`: kernel linker script
- `uefi/`: OVMF firmware files
- `run.sh`: Linux build, image, and QEMU launcher
- `run.ps1`: Windows helper script with WSL-based image handling
- `LICENSES/`: third-party license texts
- `THIRD_PARTY_NOTICES.md`: bundled third-party asset notices

Generated files live under `build/`, `LineOS/`, and `logs/`.

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
- `mkfs.ext3`
- `losetup`
- OVMF firmware

KVM is used when available.

## Running

From the project root:

```bash
./run.sh
```

The script:

1. Builds the bootloader and kernel.
2. Creates `LineOS/LineOS.img` if it does not exist.
3. Creates a GPT layout with a 300 MiB EFI partition and an EXT3 root partition.
4. Copies `BOOTX64.EFI` and `LINEOS_KERNEL.ELF` into the EFI partition.
5. Starts QEMU with Q35, VirtIO GPU, VirtIO block, OVMF, and debugcon logging.

Runtime logs:

- `logs/qemu.log`: QEMU debug log
- `logs/debugcon.log`: kernel debugcon output

## Font Rendering

LineOS currently embeds two TrueType font files into the kernel image with assembler `.incbin`:

- `kernel/assets/fonts/PTDSB.ttf`: Pretendard SemiBold
- `kernel/assets/fonts/JBMNFSB.ttf`: JetBrains Mono Nerd Font SemiBold

The kernel uses the embedded TrueType data for runtime glyph rasterization and draws directly into the VirtIO GPU framebuffer.

The current test screen renders:

- English ASCII
- digits and symbols
- Korean Hangul text
- Nerd Font private-use glyphs

The bundled fonts are third-party assets. See `THIRD_PARTY_NOTICES.md` and `LICENSES/OFL-1.1.txt`.

## Memory Notes

The kernel uses a physical page bitmap initialized from the UEFI memory map.

Important boot metadata includes:

- memory map pointer and descriptor size
- ACPI RSDP pointer
- GOP information
- loaded kernel base, size, and entry address

The kernel image is marked used during memory initialization so page allocation does not overwrite the loaded kernel or embedded font data.

## Planned Work

- VirtIO GPU cleanup and higher-level drawing API
- Better TrueType text layout and caching
- Kernel heap allocator
- EXT2 filesystem read/write support
- VirtIO block driver
- Interrupt descriptor table
- CPU exception handling
- LAPIC and timer setup
- Keyboard and mouse input
- Kernel console
- Multicore startup
- Scheduler
- User mode
- System calls
- Executable loading

## License

LineOS is licensed under the MIT License. See `LICENSE`.

Third-party fonts are licensed separately under the SIL Open Font License 1.1. See `THIRD_PARTY_NOTICES.md`.
