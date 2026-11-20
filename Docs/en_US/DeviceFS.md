# DeviceFS and userspace ABI

[Documentation index](README.md) · [한국어](../ko_KR/DeviceFS.md) · [English](../en_US/DeviceFS.md)

## Runtime and syscall ABI

Project musl supplies standard C/POSIX APIs, TLS and thread-local errno. `LOSLib` (`liblos.so`) supplies only LOS APIs. The kernel implements a subset of Linux x86-64 syscalls; a function existing in musl does not imply kernel support. `RAX` carries the number/result; arguments use `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`. `RCX` and `R11` are clobbered. Ordinary errors are negative errno values, converted by libc. `brk` instead returns the previous break on failure.

The authoritative dispatch is `Kernel/Source/Syscall/Syscall.c`, with numbers in `Common/Syscall/Numbers.h`. See the [syscall inventory](Syscalls.md) for numbers and limits.

## Files and descriptors

Each process has 16 descriptor slots. Fork and F_DUPFD share open-file offsets/status flags; CLOEXEC belongs to each descriptor. Successful exec closes CLOEXEC descriptors; failed exec preserves the old state. Regular-file identity checks use path, partition, inode and inode generation. Unrelated directory changes no longer invalidate descriptors. Open-inode lifetime across rename/unlink is still not fully POSIX-compatible.

Open/openat support access modes, CREATE, EXCL, TRUNC, APPEND, DIRECTORY, NOFOLLOW and CLOEXEC, accepting x86-64 O_LARGEFILE. Openat resolves relative paths against AT_FDCWD or a directory descriptor and ignores dirfd for absolute paths. O_PATH, O_TMPFILE and other unsupported flags are rejected. Pread64 reads regular files without moving the shared offset; access checks real UID/GID.

Stat/lstat/fstat/newfstatat use the 144-byte Linux x86-64 stat layout. Newfstatat handles AT_SYMLINK_NOFOLLOW and AT_EMPTY_PATH and accepts AT_NO_AUTOMOUNT. Getdents64 produces Linux dirent64 records used by musl directory APIs.

Fcntl supports GETFD, SETFD, GETFL, SETFL, DUPFD and DUPFD_CLOEXEC. SETFL handles APPEND and regular-file NONBLOCK status, but device asynchronous/nonblocking I/O, file locks and O_DIRECT are unsupported. Poll only validates descriptors with timeout=0 and events=0; general readiness waiting is not implemented.

## DeviceFS and terminal

DeviceFS mounts at `/dev` with fixed nodes including `/dev/null`, `/dev/fb0`, `/dev/lsh1` and `/dev/lsh1-master`. Device operations provide open/read/write/close/ioctl/map; the final open-object reference closes the device. Dynamic registration and mknod are unsupported.

Init opens the program endpoint as descriptors 0–2 and the master as descriptor 3. Only Terminal inherits the master; Login and the shell use program endpoints. Separate 64 KiB input/output queues block empty reads and full writes on Wait Queues. Only boot Init may open the master; final master close allows input to drain to EOF. Ioctl commands are normalized to their low 32 bits for musl signed-int compatibility.

Framebuffer controls include Linux screen-information requests and LOS flush/present, display ownership and input forwarding. Fb0 MAP_SHARED shares framebuffer RAM and writable mappings require a writable FD. Executable/private framebuffer mappings, dynamic modes, DRM/KMS and vsync are unsupported. Writes through mappings require LOS flush/present. See [lwm](Lwm.md) and [IPC](IPC.md) for ownership and GUI behavior.

## mmap and brk

Anonymous MAP_PRIVATE, regular-file MAP_PRIVATE, framebuffer MAP_SHARED, PROT_NONE and mprotect are supported. Regular-file mapping eagerly copies bytes into private pages and zero-fills the remainder without changing the FD offset. It does not synchronize subsequent disk changes or implement Linux SIGBUS beyond EOF. Regular-file MAP_SHARED, msync and writeback are unsupported.

The arena is `0x600000000000..0x600100000000`, with 64 MiB per call and 128 MiB total reserved spans per process. MAP_FIXED and MAP_FIXED_NOREPLACE operate within this arena. Replacement is limited to supported existing private mappings, not arbitrary ownership classes. Partial munmap and holes are handled; ELF/asset pages outside the arena cannot be arbitrarily unmapped. VMM locking and SMP TLB shootdowns synchronize shared address-space updates.

Brk(0) queries the current break; success returns the requested address and failure returns the previous break. The heap is limited to 64 MiB from its initial break. Musl malloc uses these kernel facilities.

## Processes, threads and ELF

Programs are dynamic PIE ELF images. The kernel maps PT_INTERP and builds a Linux argc/argv/envp/auxv initial stack. The default `/lib/libc.so` interpreter loads shared libraries and prepares TLS. The separate LineOS loader remains for experimentation/validation. This does not provide arbitrary Linux binary compatibility; shebang scripts are unsupported.

PROCESS shares the address space, descriptors, cwd, credentials and mappings. TASK owns TID, registers, FS base, kernel stack and signal mask. Clone/clone3 support thread flag combinations, FS-based TLS, private FUTEX_WAIT/WAKE, clear_child_tid and robust lists. Basic musl pthread creation/join works; signal delivery, asynchronous cancellation and the full futex operation set do not. Rt_sigprocmask handles per-thread 64-bit BLOCK/UNBLOCK/SETMASK and queries, not handler delivery. Membarrier QUERY returns a zero supported-command mask.

Fork eagerly copies private pages without COW, sharing open objects, framebuffer and shared assets. Fork/execve from multithreaded processes return EBUSY. Exec prepares a replacement before committing and preserves PID. Limits: 64 argv and 64 envp entries, 4096 bytes per string, 16384 combined bytes. Process execution records use 128 slots. Wait4 supports child PID or -1, WNOHANG and one-time status collection, not process groups, rusage or reparenting.

Root-owned setuid execution, AT_SECURE and trusted-path checks are implemented; setgid execution is unsupported. See [users and sudo](Users.md) for the custom authentication model. Memfd_create/sealing, general poll waits, socket syscalls, job control and ordinary signal delivery remain unsupported.
