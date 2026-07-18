# LineOS Development

**LineOS** is a hobby operating system written primarily in pure C and built for 64-bit UEFI systems using EDK II.

The project is focused on learning low-level system programming, UEFI boot processes, kernel development, memory management, graphics output, and hardware initialization.

## Overview

LineOS currently uses a custom UEFI bootloader to load and start a 64-bit kernel.

The bootloader gathers system information from UEFI and passes it to the kernel through a `LINEOS_BOOT_INFO` structure.

Current boot information includes:

* Graphics Output Protocol information
* UEFI memory map
* ACPI information
* Kernel loading information

The kernel currently boots successfully and can access the GOP framebuffer.

## Architecture

* Architecture: `x86_64`
* Firmware interface: `UEFI`
* Execution mode: `64-bit Long Mode`
* Bootloader environment: `EDK II`
* Kernel language: `C`
* Kernel format: `ELF64`
* BIOS support: Not supported

LineOS does not use 16-bit BIOS boot code.

## Project Structure

```text
LineOS/
├── bootloader/
│   ├── edk2/
│   ├── include/
│   ├── lib/
│   ├── src/
│   └── entry.c
├── kernel/
│   ├── include/
│   │   └──fontbitmaps/
│   └── assets/
├── link/
│   └── link.ld
├── LICENSE
├── makefile
├── run.ps1
├── .gitignore
└── README.md
```

## Current Features

* UEFI bootloader
* ELF64 kernel loading
* Kernel entry-point handoff
* GOP framebuffer information
* UEFI memory map collection
* ACPI table discovery
* Boot information structure
* Basic framebuffer drawing
* 64-bit kernel execution

## Planned Features

* Kernel console
* Bitmap font rendering
* Physical memory manager
* Virtual memory manager
* GDT and IDT initialization
* Interrupt handling
* APIC support
* Timer support
* Keyboard input
* Dynamic memory allocation
* Multitasking
* File system support
* User mode
* System calls

## Building

LineOS is developed using EDK II for the UEFI bootloader and a freestanding C toolchain for the kernel.

Exact build instructions will be added as the build system becomes more stable.

Typical requirements include:

* EDK II
* GCC or Clang cross-compiler
* GNU Binutils
* QEMU
* OVMF
* Git

## Running

LineOS is intended to run under QEMU with OVMF firmware during development.

Example execution commands will be added once the disk image and build process are finalized.

## Development Status

LineOS is currently under active development.

The project is experimental and is not intended for production use. APIs, boot structures, memory layouts, and directory structures may change frequently.

## Goals

The main goals of LineOS are:

* Learn how UEFI operating systems boot
* Build a kernel without depending on an existing operating system
* Understand low-level memory and hardware management
* Create a clean and maintainable C codebase
* Gradually develop a usable 64-bit hobby operating system

## License

Copyright © 2026 LineOS Developer `kljj04`.

See the LICENSE file.