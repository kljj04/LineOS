# Building and running

[Documentation index](README.md) · [한국어](../ko_KR/Build.md) · [English](../en_US/Build.md)

Linux is the primary development environment. Commands below run from the project root. The launcher is
`Scripts/Run.sh`; the Make entry point is `Makefiles/Makefile`.

## Requirements

- Build: Bash, `make`, `clang`, `ld.lld`, `lld-link` and `llvm-ar`.
- Builder interface: `node`, using built-in Node.js modules; no npm install is required.
- Disk preparation: `sudo`, `truncate`, `parted`, `losetup`, `mkfs.vfat`, `mkfs.ext2`, `mount`, `umount`, `findmnt`,
  `fuser`, `install` and standard Linux file utilities.
- VM: `qemu-system-x86_64`, accessible `/dev/kvm`, the configured host CPU features, and bundled `UEFI/OVMF_CODE.fd` and
  `UEFI/OVMF_VARS.fd`.
- Formatting: `clang-format`.

The launcher does not automatically fall back from KVM to software emulation.

## Commands

```bash
./Scripts/Run.sh --help
./Scripts/Run.sh -build
./Scripts/Run.sh
```

| Option             | Action                                                                                          |
|--------------------|-------------------------------------------------------------------------------------------------|
| No option or `all` | Unmount managed image → build/update image → QEMU → remount EFI and root                        |
| `-build`           | Incremental build only                                                                          |
| `-image`           | Prepare image if absent and copy existing build artifacts; does not build                       |
| `-m`, `-mount`     | Mount the existing image at `LineOS/Mount/EFI` and `LineOS/Mount/Root`; no build or file update |
| `-u`, `-umount`    | Unmount managed EFI/root directories and detach their loop devices                              |
| `-qemu`            | Unmount → QEMU → remount; does not build or update the image                                    |
| `-clean`           | Remove build objects and the two boot artifacts; retain the disk image                          |
| `-format`          | Format project C/header sources in place, excluding bundled EDK2                                |
| `-h`, `--help`     | Show help                                                                                       |

The Builder invokes Make sequentially with `-j1` and shows one active `[total/current] path` row, stage timings and QEMU
termination diagnostics. Non-interactive output uses ordinary lines.

Direct build without the Node.js interface:

```bash
make -f Makefiles/Makefile -j1
```

Outputs include `LineOS/EFI/BOOT/BOOTX64.EFI`, `LineOS/KERNEL/LINEOS_KERNEL.ELF` and
`LineOS/Programs/<relative program path>/<directory name>.lix`. Objects and dependency files are under `Build/Objects/`.
The build recursively discovers directories containing `UserPrograms/**/Program.cfg`. Each complete relative path identifies
one program; sources belonging to nested program roots are excluded from their parent. Executables remain static ELF64.

```ini
CopyPath = /sbin/
Dependencies = TrueType, UserDB
Enable = true
```

Whitespace around keys, values and commas is allowed. Omitted values default to `CopyPath=/bin/`, an empty dependency
list and `Enable=true`. LOSLibC is always linked; optional library names are `TrueType`, `UserDB` and `LOSLibC`.
Unknown keys/dependencies, duplicate keys and invalid values are errors. `Enable=false` excludes a program from both
build and deployment. Existing installed files are not removed automatically.

```bash
./Scripts/Run.sh -build System/Shell
make -f Makefiles/Makefile -j1 UserProgram Program=System/Shell
```

These commands build only the selected enabled program and its libraries. Deployment uses `CopyPath/<directory name>.lix`;
all enabled destinations, including ancestor conflicts and font destinations, are checked before disk copies begin.
Every full or selected build clears `LineOS/Programs/` before linking its target executables again. Objects and libraries
remain incremental. A selected build leaves only the selected executable.
Only outputs associated with current enabled configurations are copied. `-clean` also removes `LineOS/Programs/`, including
stale outputs. Building alone neither updates the image nor starts QEMU.

## Configuration

`Configs/QEMUSettings.sh` is sourced as Bash by `Scripts/Backend/Config.sh` and validated before build/image/QEMU
operations. Edit this file to change the VM; arrays use Bash array syntax. The former TOML configuration and
`LINEOS_DISPLAY` override are not used by the current launcher.

| Setting                                  | Current value                                                           |
|------------------------------------------|-------------------------------------------------------------------------|
| `MachineType` / `MachineAcceleration`    | `q35` / `kvm`                                                           |
| `CPUModel` / `CPUCount`                  | `host` / `16`                                                           |
| `CPUFlags`                               | `+tsc-deadline`, `+invtsc`, `+rdtscp`, `+arat`, `+fsgsbase`, `+pdpe1gb` |
| `MemorySize`                             | `4G`                                                                    |
| `GraphicsDisplay` / `GraphicsResolution` | `gtk` / `2560x1440`                                                     |
| `GraphicsDevice` / `GraphicsHostMemory`  | `virtio-gpu-pci` / `1G`                                                 |
| `NetworkBackend` / `NetworkMAC`          | `user` / `52:54:00:4c:4f:53`                                            |
| `InputKeyboard` / `InputMouse`           | `virtio-keyboard-pci` / `virtio-tablet-pci`                             |
| `RTCBase` / `RTCClock`                   | `utc` / `host`                                                          |
| `DiskImage` / `DiskSize`                 | `LineOS/LineOS.img` / `8G`                                              |
| `DiskEFISizeMiB`                         | `300`                                                                   |
| `DebugFlags`                             | `int`, `guest_errors`, `cpu_reset`, `unimp`                             |
| `DebugNoReboot` / `DebugNoShutdown`      | `true` / `true`                                                         |

`DiskSize` and `DiskEFISizeMiB` apply only when creating a missing image; changing them does not resize an existing
disk. QEMU user NAT needs no host bridge or TAP configuration. See [networking](Network.md).

## Image lifecycle and privileges

Run the launcher as the desktop user. The full workflow prompts through `sudo -v` in the foreground before releasing
managed mounts and building; subsequent disk operations use non-interactive sudo. QEMU runs as the desktop user.

A missing image is prepared as a temporary raw GPT disk, with FAT32 ESP partition 1 from 1 MiB through 301 MiB and EXT2
partition 2 using the remaining space. It is published without overwriting an existing path. An existing image is reused
without formatting. The update copies `EFI/BOOT/BOOTX64.EFI` and `KERNEL/LINEOS_KERNEL.ELF` to the ESP, then copies user
programs and `Assets/Fonts/*.ttf` to EXT2. Program destinations come from `Program.cfg` and default to `/bin/`; fonts go
to `/sys/fonts/`. See [program deployment](UserLibraries.md). Existing EXT2 is not formatted. Managed mounts created by
`-mount` are unmounted and detached before updating the image. Other existing loop attachments are rejected. Cleanup
unmounts and detaches the loop device; failures identify resources left for inspection.

## Logs and firmware state

| File                                | Contents                                  |
|-------------------------------------|-------------------------------------------|
| `Logs/Build.log`                    | Build output                              |
| `Logs/Format.log`, `Logs/Clean.log` | Output from the corresponding operation   |
| `Logs/Image.log`, `Logs/Mount.log`  | Image preparation, update or manual mount |
| `Logs/Remount.log`                  | Image remount after QEMU exits            |
| `Logs/QEMU.log`                     | Configured QEMU diagnostics               |
| `Logs/DebugCon.log`                 | Kernel port `0xE9` output                 |
| `Logs/QEMUProcess.log`              | QEMU process output                       |

Each stage overwrites its own log when run. QEMU logs are reset at launch; messages depend on the current kernel
workload. The launcher copies `UEFI/OVMF_VARS.fd` into a temporary directory for each VM run, leaving the original
variable template unchanged. The normal disk is writable; use a separate image when disk-state isolation is required.

## Source map

`Scripts/Run.sh` coordinates the stages. `Scripts/Backend/` contains configuration, build, image, mount and QEMU
functions. `Scripts/Frontend/UI.js` renders the interface. Compiler/linker flags and targets are in `Makefiles/`;
C/header formatting is configured by `.clang-format`.

## Manual mount

`./Scripts/Run.sh -mount` (also `-m`, `-Mount` or `-M`) mounts both partitions of the existing image and leaves them
mounted after exit. The EFI filesystem is accessible to the invoking user; EXT2 retains its existing ownership and
permissions. No formatting or build-file copying is performed. The option refuses an image open by another process.

A later `-mount`, image update, full run or `-qemu` invocation releases these managed mounts first. If unmounting fails
because a directory or file is busy, the operation stops. Mounts outside `LineOS/Mount/EFI` and `LineOS/Mount/Root` are
not automatically released. Use `./Scripts/Run.sh -umount` (also `-u`, case-insensitive) to explicitly release the
managed mounts and loop devices.

The full run and `-qemu` restore both mounts after QEMU exits, including launch failures and handled SIGINT/SIGTERM. The
launcher stops and waits for the QEMU process before mounting the disk again. Restoration refreshes sudo authentication
if needed and records output in `Logs/Remount.log`. A remount failure is reported as an error; an existing QEMU failure
code is preserved. Forced termination such as SIGKILL cannot run cleanup; stop any surviving QEMU process, then use
`-umount` to release remaining managed mounts before retrying.

Each manual or post-QEMU mount is prepared under a temporary `.Mount.*` directory. After both partitions are mounted,
that populated directory replaces `LineOS/Mount`, emitting a directory replacement event for IDE file watchers. Only
empty old mount-point directories are removed; unexpected files cause the operation to stop without deleting them.
