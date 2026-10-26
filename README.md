[![Arch](https://img.shields.io/badge/Arch-x86__64-24292f?style=flat-square)](https://github.com/kljj04/LineOS)
[![Language](https://img.shields.io/badge/Language-C-3465a4?style=flat-square)](https://github.com/kljj04/LineOS)
[![Boot](https://img.shields.io/badge/Boot-UEFI-555555?style=flat-square)](https://github.com/kljj04/LineOS)
[![Kernel](https://img.shields.io/badge/Kernel-Monolithic-24292f?style=flat-square)](https://github.com/kljj04/LineOS)
[![Platform](https://img.shields.io/badge/Platform-Bare_Metal-555555?style=flat-square)](https://github.com/kljj04/LineOS)

![Last Commit](https://img.shields.io/github/last-commit/kljj04/LineOS?style=flat-square)
![Repo Size](https://img.shields.io/github/repo-size/kljj04/LineOS?style=flat-square)
![Commits](https://img.shields.io/github/commit-activity/t/kljj04/LineOS?style=flat-square)

# LineOS

LineOS is an experimental x86_64 UEFI operating system written primarily in freestanding C and assembly. It explores multicore kernel development, memory management, device drivers, graphics, storage and networking.

The current development environment is Linux with QEMU/KVM and VirtIO devices. Physical hardware support depends on the available drivers; the project is not yet a general-purpose desktop operating system.

## Project overview

- Custom UEFI bootloader and ELF64 monolithic kernel
- SMP startup, APIC interrupts, hardware timers and the LBPWRR scheduler with Wait Queues and CPU usage accounting
- Physical and virtual memory management with a kernel heap
- Shared PCI/VirtIO infrastructure for GPU, input, block and network devices
- Native TrueType text rendering, including Hangul
- GPT, EXT2 and a VFS layer
- IPv4 networking with ARP, ICMP, DHCP, UDP, DNS and a TCP client with up to 16 concurrent connections

The kernel mounts EXT2 and starts the Ring 3 `Init.lix` program. Init publishes shared font assets and starts `Shell.lix`; Shell maps those assets read-only, reads the shared output stream and displays ASCII text using the userspace TrueType library. It is an output demonstration, not an interactive command shell. The system call interface and process lifecycle are still incomplete. Networking and NTP workers are implemented but are not registered by the default startup path.

## Getting started

On a Linux development host with the required compiler, disk tools and QEMU/KVM:

```bash
./Scripts/Run.sh -build
./Scripts/Run.sh
```

The Bash launcher uses a Node.js interface and sequential Make builds. It prepares the boot image using `sudo` and starts QEMU/KVM. VM settings are in `Configs/QEMUSettings.sh`; networking defaults to QEMU user NAT without host bridge setup. See [build and run instructions](Docs/en_US/Build.md) for requirements and configuration.

## Documentation

[한국어 문서](Docs/ko_KR/README.md) · [English documentation](Docs/en_US/README.md)

The [documentation index](Docs/README.md) links to architecture, memory, scheduling, devices, storage, networking and coding conventions. It distinguishes implemented subsystems from the current startup workload.

## License

LineOS is licensed under the POL License. See [LICENSE](LICENSE.md).

Bundled fonts and other third-party material have separate terms. See [third-party notices](Docs/THIRD_PARTY_NOTICES.md).
