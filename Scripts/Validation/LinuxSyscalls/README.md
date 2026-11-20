# Linux syscall validation

Run `bash Scripts/Validation/LinuxSyscalls.sh` for ASan/UBSan host tests of the actual syscall and VFS sources. The host fixture substitutes allocation, task, user-copy, page-table and FS backend operations. The temporary VFS source disables privileged interrupt instructions; this test does not validate kernel synchronization or page-table code.

Run `bash Scripts/Validation/LinuxSyscallsGuest.sh` on the development host to build static and project musl guest fixtures in `Build/Validation/LinuxSyscalls`. This does not boot a VM or modify the disk image.

On a disposable copy of the boot image, install the current kernel, put `Payload` at `/mmap-fixture`, and install either `Guest.lix` or `GuestMusl.lix` at `/sbin/Init.lix` with mode 0755. The musl fixture needs the built `LineOS/Libraries/libc.so` at `/lib/libc.so` in the guest. Start from a clean EXT2 filesystem and remove previous result files before each run.

The fixture writes an eight-byte result to `/linux-syscalls-PASS` (zero) or `/linux-syscalls-FAIL` (the failing source line), closes the result file, then yields indefinitely. Stop the VM and inspect the disposable filesystem. The musl variant first calls musl's `getpid()` through its PLT, validating library discovery, mapping and symbol resolution without claiming complete libc startup compatibility.

Validated on 2026-11-20: full build, host sanitizer tests, existing DynamicLoader host tests, and both static and glibc fixtures under 2-vCPU KVM. The glibc fixture also emitted unimplemented syscall 334 (`rseq`) and 302 (`prlimit64`), then completed successfully.

## Limits

- Regular file mappings use eager private copies; `MAP_SHARED` writeback, page cache coherence, demand paging and SIGBUS for whole pages beyond EOF are unsupported. Pages beyond EOF are currently zero-filled.
- `MAP_FIXED` is restricted to the existing mmap address window and can replace owned mmap pages. Replacement of executable image, heap or device/shared mappings is rejected.
- Individual mappings and the heap are limited to 64 MiB; mapped-page accounting retains the existing 128 MiB process limit.
- Unsupported open flags, `O_PATH`, special mode bits and Linux umask semantics remain outside this change. Existing VFS handles are path-based: rename/unlink durability is not equivalent to Linux inode-backed FDs.
- Access checks use the existing single UID/GID credential model; supplementary groups, ACLs and Linux capabilities are unsupported.
