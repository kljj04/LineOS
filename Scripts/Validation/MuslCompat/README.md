# musl compatibility regression tests

`bash Scripts/Validation/MuslCompat.sh` builds the current system and two disposable-VM fixtures. Install `Build/Validation/MuslCompat/Guest.lix` as `/sbin/Init.lix`, `Child.lix` as `/compat-child.lix`, all current shared libraries in `/lib`, and the font files referenced by Init's Publish configuration in a copied disk image. Use the current kernel. Never replace Init on the normal image for this test.

The guest checks signal-mask block/query/restore and fork/exec inheritance, membarrier capability query, FD duplication and descriptor-specific CLOEXEC, file status flags, open-FD read/mmap after unrelated namespace mutations, stale-handle rejection after replacement, and the actual application's LwmOpen/LwmCompose paths. A separate child acknowledges the existing terminal graphics handoff through LOS_TIO_WAIT_READ. High-bit pointer/compositor ioctl requests pass through musl's signed int interface.

Success writes `/musl-compat-PASS`: two signed 64-bit values, result=0 and the last errno value. Successful operations need not reset errno, so the second value can retain an expected ESTALE from the negative test. Failure writes `/musl-compat-FAIL`: failing source line and errno. A 2-vCPU KVM run passed these checks on 2026-11-20.

`python3 Scripts/Validation/Interactive.py` runs the actual LoginReadLine code with fully buffered output, verifies that prompts reach the output before input, checks password no-echo, and executes the actual lsh objects with musl to verify that cursor-control output remains inside each synchronized redraw frame.

`bash Scripts/Validation/LinuxSyscalls.sh` covers the kernel syscall implementations with ASan/UBSan stubs, including mask validation, per-FD flags, duplication and signed ioctl normalization. It also tests EXT2's real inode generation write path for persistence, reuse increments and overflow rejection.

`rt_sigprocmask` stores per-task masks; this change does not implement signal handlers or signal delivery. fcntl record locks, async/direct I/O and device nonblocking mode remain unsupported. membarrier advertises no supported commands through QUERY; other commands return EINVAL. memfd_create and sealing remain unsupported.
