# LineOS documentation

[한국어](../ko_KR/README.md) · [English](../en_US/README.md) · [Project](../../README.md)

Start with build and run, then choose a subsystem. Source paths and commands are relative to the project root.

Configuration and startup were checked against the checkout on 2026-10-24. Start with `./Scripts/Run.sh --help`; VM settings are in `Configs/QEMUSettings.sh`. The default workload is CPU usage display plus mouse handling.

| Document | Contents |
| --- | --- |
| [Architecture and boot](Architecture.md) | Startup, SMP, interrupts and source map |
| [Build and run](Build.md) | Requirements, disk image, VM configuration and logs |
| [Memory](Memory.md) | PMM, heap, VMM and ownership |
| [Scheduler](Scheduler.md) | LBPWRR, Wait Queues, CPU usage, task lifetime and migration |
| [Devices and rendering](Devices.md) | PCI, VirtIO, GPU, input and fonts |
| [Storage and filesystems](Storage.md) | Block devices, GPT, EXT2, VFS and timestamps |
| [Networking](Network.md) | VirtIO-Net, NAT, DHCP, UDP, DNS and TCP |
| [Clock and NTP](Clock.md) | RTC, UTC, uptime and optional NTP worker |
| [Coding conventions](Directions.md) | Naming, formatting and kernel macros |

[Third-party notices](../THIRD_PARTY_NOTICES.md) and `Docs/LICENSES/` are shared. Each subsystem page describes its implementation scope.
