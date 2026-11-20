# Validation

[Documentation index](README.md) · [한국어](../ko_KR/Validation.md) · [English](../en_US/Validation.md)

Build success, host tests and disposable-VM observations are different evidence. A guest-fixture builder does not itself boot QEMU or validate the disk image. VM results apply to the kernel, libraries, settings and image used for that run.

## Host checks

Run from the repository root after building the relevant sources:

```bash
make -f Makefiles/Makefile -j8 All
python3 Scripts/Validation/Libraries.py
python3 Scripts/Validation/Programs.py
python3 Scripts/Validation/Locale.py
python3 Scripts/Validation/Interactive.py
bash Scripts/Validation/LinuxSyscalls.sh
```

| Check | Coverage |
| --- | --- |
| Libraries.py | Configuration/collision/cycle checks; real freestanding ELF builds; dependency execution and RUNPATH; custom make, Artifact and dry-run |
| Programs.py | Configuration and generated constructors with sanitizers; deployment preflight/CopyPath; stale staged-file exclusion; Name/PT_INTERP linking |
| Locale.py | Real musl refresh, environment precedence, translation and stored names, with temporary configuration paths |
| Interactive.py | Actual Login prompt flushing/password hiding and ANSI redraw ordering from real musl-linked lsh objects |
| LinuxSyscalls.sh | Actual syscall/VFS source under ASan/UBSan, substituting allocation, tasks, storage and user-copy operations; not real page-table/IRQ synchronization |

Read-only configuration/build planning checks:

```bash
bash Scripts/Backend/Libraries.sh .
bash Scripts/Backend/Programs.sh .
make -f Makefiles/Makefile -n All
```

## Guest fixture builders

| Command | Output and instructions |
| --- | --- |
| `bash Scripts/Validation/LinuxSyscallsGuest.sh` | Build/Validation/LinuxSyscalls; [syscall fixture](../../Scripts/Validation/LinuxSyscalls/README.md) |
| `bash Scripts/Validation/MuslCompat.sh` | Build/Validation/MuslCompat; [musl regression fixture](../../Scripts/Validation/MuslCompat/README.md) |
| `python3 Scripts/Validation/ProgramConfigGuest.py` | Build/Validation/ProgramConfig; deployment below |
| `bash Scripts/Validation/SecureExec.sh` | Build/Validation/SecureExec; setuid, AT_SECURE, loader injection and real sudo authentication |

## Program.cfg guest fixture

In a disposable image, install ConfigInit.lix as `/sbin/Init.lix`, Consumer.lix as `/cfg-consumer.lix`, Missing.lix as `/cfg-missing.lix`, Denied.lix as `/cfg-denied.lix` and Payload as `/cfg-payload`. Also install the current kernel, `/lib/libc.so` and `/lib/liblos.so`.

Success writes an eight-byte zero to `/program-cfg-PASS`. `/program-cfg-FAIL` contains 1 for publish/acquire failure, 2 for consumer failure, 3 if missing-asset startup was not rejected or 4 if an unauthorized publisher was not rejected.

## Running and reporting VM checks

Use a disposable image with current binaries, required libraries and fixture-specific files/accounts. Never replace production Init or real account data with fixtures. Secure-exec tests additionally require fixture identities and root ownership/modes; copying only an executable is insufficient.

Remove prior result records, run the copied VM, shut it down and inspect fresh EXT2 PASS/FAIL files. Record the failing source line and errno where supplied. Explicitly state which commands were run, which substitutions were used and whether the VM was actually observed.

The musl and standalone-loader READMEs retain historical test records. A historical glibc fixture pass does not select glibc as the current runtime. An optional test reporting ENOSYS, such as memfd/sealing, is not evidence that the feature is supported.
