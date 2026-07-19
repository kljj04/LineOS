# LineOS Development

**LineOS** is a hobby operating system written primarily in pure C for 64-bit UEFI systems.

The project uses EDK II for its custom UEFI bootloader and a freestanding C environment for the kernel. Its main purpose is to explore low-level system programming, kernel development, graphics rendering, memory management, hardware initialization, and modern x86_64 platform architecture.

## Overview

LineOS currently uses a custom UEFI bootloader to load an ELF64 kernel and transfer execution to it.

Before entering the kernel, the bootloader gathers platform information from UEFI and passes it through a `LINEOS_BOOT_INFO` structure.

The information currently passed to the kernel includes:

* Graphics Output Protocol information
* Framebuffer address and display properties
* UEFI memory map
* ACPI root table information
* Kernel boot metadata

The kernel can initialize the framebuffer, render graphics, and display Unicode text using an embedded bitmap font.

## Architecture

* Architecture: `x86_64`
* Firmware interface: `UEFI`
* Execution mode: `64-bit Long Mode`
* Bootloader environment: `EDK II`
* Kernel language: `C`
* Kernel format: `ELF64`
* Text encoding: `UTF-16`
* Graphics output: `UEFI GOP framebuffer`
* BIOS support: Not supported

LineOS does not use legacy BIOS services or 16-bit boot code.

## Current Features

* Custom UEFI bootloader
* ELF64 kernel loading
* Kernel entry-point handoff
* GOP framebuffer initialization
* Screen and rectangle drawing
* Pixel and line rendering
* 8-bit alpha-blended bitmap font rendering
* UTF-16 text output
* ASCII, symbols, box-drawing characters, Hangul Jamo, and complete Hangul syllable support
* Korean and English kernel text rendering
* UEFI memory map collection
* ACPI RSDP discovery
* Boot information structure
* Basic CPU control functions
* Experimental boot and kernel panic screens

## Font System

LineOS contains an embedded bitmap font generated from multiple source fonts.

The current font configuration uses:

* JetBrains Mono SemiBold for ASCII and symbols
* Pretendard SemiBold for Korean characters
* 8-bit grayscale alpha values for antialiasing
* Per-glyph metrics including size, advance, and offset
* A sorted Unicode table for glyph lookup

The font currently includes the full set of modern Hangul syllables, allowing Korean text to be rendered directly by the kernel.

Because the complete font data is embedded in the kernel image, it currently represents a significant portion of the kernel size.

## Planned Features

* Physical memory manager
* Virtual memory manager
* Kernel heap allocator
* GDT initialization
* IDT initialization
* CPU exception handlers
* Kernel panic diagnostics
* Register and stack dumps
* Local APIC initialization
* I/O APIC support
* HPET initialization
* TSC calibration
* TSC-deadline timer support
* LAPIC timer fallback
* Keyboard input
* Interactive kernel console
* Multicore processor initialization
* Task scheduling
* Multitasking
* File system support
* Storage device drivers
* User mode
* System calls
* Executable loading

## Memory Requirements

The currently tested minimum memory configuration is approximately `60 MiB`.

For practical use, the recommended minimum is:

* Minimum: `64 MiB`
* Recommended: `128 MiB` or more

A significant amount of memory is currently used by the embedded 8-bit bitmap font and the graphical framebuffer.

These requirements may change as the kernel and font storage system are optimized.

## Building

LineOS uses EDK II for the UEFI bootloader and a freestanding C toolchain for the kernel.

Typical build requirements include:

* EDK II
* GCC or Clang
* GNU Binutils
* QEMU
* OVMF
* Git
* PowerShell

The project is currently developed and tested primarily on Windows using PowerShell scripts and QEMU.

Detailed build instructions will be added as the build system becomes more stable.

## Running

LineOS is primarily tested under QEMU using OVMF firmware.

The current development environment boots the UEFI loader, loads the kernel, initializes the framebuffer, and displays the graphical kernel interface.

Real hardware support has not yet been extensively tested.

## Development Status

LineOS is under active development.

The project is experimental and is not intended for production use. Internal APIs, boot structures, memory layouts, rendering interfaces, and build procedures may change frequently.

The current development focus is on:

* Framebuffer rendering
* Unicode text output
* Kernel diagnostics
* CPU initialization
* Memory management
* Interrupt and timer infrastructure

## Goals

The main goals of LineOS are:

* Learn how modern UEFI operating systems boot
* Build a kernel without depending on an existing operating system
* Understand x86_64 processor and platform architecture
* Explore low-level memory and hardware management
* Build a graphical kernel interface
* Support Korean text directly inside the kernel
* Maintain a clean and understandable pure C codebase
* Gradually develop a usable 64-bit hobby operating system

## License

Copyright © 2026 LineOS Developer `kljj04`.

See the `LICENSE` file for details.