# Building and running

[Documentation index](README.md) · [한국어](../ko_KR/Build.md) · [English](../en_US/Build.md)

Linux is the primary development environment.

Typical requirements:

- `clang`
- `ld.lld` and `lld-link`
- `make`
- `qemu-system-x86_64`
- `qemu-img`
- `parted`
- `mkfs.vfat`
- `mkfs.ext2`
- `losetup`
- OVMF firmware

KVM acceleration is used by the default launcher.

## Build only

```bash
make -j"$(nproc)"
```

The build produces `LineOS/EFI/BOOT/BOOTX64.EFI` and `LineOS/KERNEL/LINEOS_KERNEL.ELF`. Building alone does not copy these files into the disk image or boot QEMU.

## Run

```bash
./Run.sh
```

The Linux launcher builds the project, creates an 8 GiB raw GPT image when needed, prepares a 300 MiB FAT32 EFI partition and an EXT2 root partition, copies the boot files, and launches QEMU. Loop attachment and mounting require interactive `sudo` or equivalent local permissions.

Existing images are reused. The image path is `LineOS/LineOS.img`; boot files in that image are updated by the launcher.

## Default VM configuration

| Setting | Default |
| --- | --- |
| Machine / firmware | Q35 / bundled OVMF UEFI |
| CPU / acceleration | Host CPU, 16 vCPUs, KVM |
| Memory | 4 GiB |
| Display | GTK, QHD 2560×1440, VirtIO GPU |
| Input | VirtIO keyboard and tablet |
| Disk | VirtIO block, raw GPT image |
| Network | VirtIO-Net, QEMU `user,id=net0` NAT |

The launcher requires usable KVM and the configured host CPU features; it does not automatically select a portable software-emulation fallback. `LINEOS_DISPLAY` overrides the display backend. Other defaults are near the beginning of `Run.sh`.

The NAT backend needs no Linux bridge, TAP device or host network reconfiguration. See [networking](Network.md).

## Logs and firmware state

QEMU diagnostics go to `Logs/QEMU.log`; kernel debug output through port `0xE9` goes to `Logs/DebugCon.log`. Log contents depend on the current kernel workload. Filesystem PASS markers from older test kernels are not expected from every boot.

The normal launcher uses `UEFI/OVMF_VARS.fd` as writable firmware state. Tests should use separate disks and copied firmware variables when the normal VM state must be preserved.
