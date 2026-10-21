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
- SMP startup, APIC interrupts, hardware timers and the LBPWRR scheduler
- Physical and virtual memory management with a kernel heap
- Shared PCI/VirtIO infrastructure for GPU, input, block and network devices
- Native TrueType text rendering, including Hangul
- GPT, EXT2 and a VFS layer
- IPv4 networking with ARP, ICMP, DHCP, UDP, DNS and a single-connection TCP client

Internal APIs and the default kernel workload are under active development. User mode, system calls, process isolation and userland applications remain future work. HTTP/HTTPS/TLS belong to that future userland layer.

## Getting started

On a Linux development host with the required compiler, disk tools and QEMU/KVM:

```bash
make -j"$(nproc)"
./Run.sh
```

The launcher prepares the boot image using `sudo` and starts a VM. Networking defaults to QEMU user NAT without host bridge setup. See [build and run instructions](Docs/en_US/Build.md) for requirements and configuration.

## Documentation

[한국어 문서](Docs/ko_KR/README.md) · [English documentation](Docs/en_US/README.md)

The [documentation index](Docs/README.md) links to architecture, memory, scheduling, devices, storage, networking, verification and coding conventions. It also distinguishes implemented subsystems from the current startup workload and historical test results.

## License

LineOS is licensed under the POL License. See [LICENSE](LICENSE.md).

Bundled fonts and other third-party material have separate terms. See [third-party notices](Docs/THIRD_PARTY_NOTICES.md).
