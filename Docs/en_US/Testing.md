# Verification and checkpoints

[Documentation index](README.md) · [한국어](../ko_KR/Testing.md) · [English](../en_US/Testing.md)

A successful compile/link, an updated boot image and an observed QEMU runtime result are separate checkpoints. A historical test result does not establish that every later kernel workload runs the same checks.

## Current network checkpoint

The QEMU user-NAT test with 16 scheduled CPUs passed DHCP address acquisition, gateway ICMP, a 1,001-byte UDP echo, external DNS lookup and a 12,000-byte TCP stream with EOF. The latest observed run reported RX 47, TX 50, zero drops and zero retransmissions. This covers the tested traffic, not full TCP compliance.

The local harness is `Tests/NetworkStack/Run.py`; [networking](Network.md) describes the protocol scope. `Tests/Network/` contains driver-level tests, while `Tests/VMM/` contains memory tests. `Tests/` is ignored by the current repository configuration, so these local harnesses may be absent from a fresh clone. Check availability before using them.

## Historical EXT2 checkpoint

The following records are retained from the previous README. They were not rerun as part of this documentation change. The current checkout has no `Tests/FS/Run.py` or default filesystem-test task in `KMain()`.

The 2026-10-19 checkpoint passed:

- 182 host image scenarios using the actual kernel EXT2/GPT sources with AddressSanitizer, UndefinedBehaviorSanitizer,
  and compiler warnings treated as errors
- Block/inode-size combinations, revision 0, triple-indirect sparse files, allocation/reclamation, path/link handling,
  permission checks, and remount persistence
- Rejection of tested corrupt metadata, unsupported features, dirty writable mounts, and invalid GPT CRCs
- Allocation, disk-write, and flush failures, plus process termination at disk-write/flush boundaries; interrupted
  images were repaired with `e2fsck` when needed and then checked again
- Actual QEMU/KVM kernel tests with both 1 and 16 virtual CPUs reaching `[EXT2 TEST] PASS`
- `e2fsck -fn` checks of both resulting QEMU EXT2 partitions without errors

The QEMU checks used separate test disks and copied firmware variables. Host process termination models an
interruption in a file-backed device; it does not reproduce every real device-cache or power-loss behavior.

The 182-scenario results above are the earlier EXT2 checkpoint. The later Clock/VFS/shell checkpoint used:

```bash
python3 Tests/FS/Run.py --work /tmp/LineOSFSTests
```

That recorded checkpoint passed 13 clock checks, 144 filesystem/VFS checks for each of 1/2/4 KiB blocks, and 117 shell
checks under ASan/UBSan. QEMU/KVM with 1 and 16 CPUs also passed the kernel tests and 27 actual keyboard-driven shell
commands each; resulting disks passed `e2fsck`. The former `Docs/VFS.md` is absent from the current checkout; see [storage](Storage.md) for the current implementation overview.
