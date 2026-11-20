# LineOS documentation

[Language index](../README.md) · [한국어](../ko_KR/README.md) · [Project](../../README.md)

Start with [Build and run](Build.md). Source paths and commands are relative to the repository root. Each page links to the matching Korean reference.

## Getting started

| Document | Contents |
| --- | --- |
| [Build and run](Build.md) | Tools, commands, images and IDE setup |
| [Architecture and boot](Architecture.md) | Initialization, user sessions and source map |

## Kernel

| Document | Contents |
| --- | --- |
| [Memory](Memory.md) | PMM, heap and address spaces |
| [Scheduler](Scheduler.md) | LBPWRR, waits, processes and threads |
| [Devices and rendering](Devices.md) | VirtIO, GPU, fonts and input |
| [Storage and filesystems](Storage.md) | GPT, EXT2, VFS and open-file identity |
| [Networking](Network.md) | Packets, DHCP, UDP, TCP and limits |
| [Clock and NTP](Clock.md) | UTC, monotonic time and synchronization |
| [DeviceFS and userspace ABI](DeviceFS.md) | Descriptors, mmap, ELF and threads |
| [System call inventory](Syscalls.md) | Numbers, supported operations and LOS extensions |

## Userspace

| Document | Contents |
| --- | --- |
| [Programs and libraries](UserLibraries.md) | Musl, LOSLib, TrueType and assets |
| [Accounts and sudo](Users.md) | UserDB, elevation and authentication boundaries |
| [Locale and timezone](Locale.md) | Defaults, environment and translation scope |
| [Shell variables](ShellVariables.md) | Assignments, expansion and input layout |
| [User autostart](Autostart.md) | Configuration, startup apps and session lifetime |
| [IPC and window protocol](IPC.md) | Ownership, APIs, messages and errors |
| [lwm](Lwm.md) | Window controls, input, composition and limits |

## Build configuration

| Document | Contents |
| --- | --- |
| [Program.cfg](ProgramConfig.md) | Names, deployment, dependencies and publication |
| [Library.cfg](LibraryConfig.md) | Musl/none, library graph and custom make |

## Development and validation

| Document | Contents |
| --- | --- |
| [Coding conventions](Directions.md) | Naming, formatting and file banners |
| [Validation](Validation.md) | Host checks, disposable VMs and reporting |

## Licenses

[Third-party notices](../THIRD_PARTY_NOTICES.md) and [license texts](../LICENSES/) are shared.
