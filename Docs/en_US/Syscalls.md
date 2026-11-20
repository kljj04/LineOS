# System call inventory

[Documentation index](README.md) · [한국어](../ko_KR/Syscalls.md) · [English](../en_US/Syscalls.md)

This inventory matches Common/Syscall/Numbers.h against Kernel/Source/Syscall/Syscall.c. An implemented number does not imply support for every Linux flag or operation. See [DeviceFS and ABI](DeviceFS.md) for lifetime/error rules and [Validation](Validation.md) for evidence. LOS extensions begin at 1024.

## Linux x86-64 subset

| Number | Name | Scope |
| --- | --- | --- |
| 0 | `read` | Implemented subset; see ABI/source |
| 1 | `write` | Implemented subset; see ABI/source |
| 2 | `open` | Implemented subset; see ABI/source |
| 3 | `close` | Implemented subset; see ABI/source |
| 4 | `stat` | Implemented subset; see ABI/source |
| 5 | `fstat` | Implemented subset; see ABI/source |
| 6 | `lstat` | Implemented subset; see ABI/source |
| 7 | `poll` | timeout=0, events=0 FD validity only |
| 9 | `mmap` | Anonymous/private file, shared fb0; bounded fixed mappings |
| 10 | `mprotect` | Implemented subset; see ABI/source |
| 11 | `munmap` | Implemented subset; see ABI/source |
| 12 | `brk` | Failure returns previous break |
| 14 | `rt_sigprocmask` | 64-bit per-thread mask; no signal delivery |
| 16 | `ioctl` | Implemented subset; see ABI/source |
| 17 | `pread64` | Regular-file read; preserves offset |
| 20 | `writev` | Implemented subset; see ABI/source |
| 21 | `access` | Implemented subset; see ABI/source |
| 24 | `yield` | Implemented subset; see ABI/source |
| 35 | `nanosleep` | Implemented subset; see ABI/source |
| 39 | `getpid` | Implemented subset; see ABI/source |
| 56 | `clone` | Thread flag combinations only |
| 57 | `fork` | Eager copy; multithreaded process EBUSY |
| 59 | `execve` | ELF/PT_INTERP; no shebang; multithreaded process EBUSY |
| 60 | `exit` | Implemented subset; see ABI/source |
| 61 | `wait4` | Child PID or -1; WNOHANG; no groups/rusage |
| 72 | `fcntl` | FD flags, file status subset, DUPFD; no locking |
| 79 | `getcwd` | Implemented subset; see ABI/source |
| 80 | `chdir` | Implemented subset; see ABI/source |
| 82 | `rename` | Implemented subset; see ABI/source |
| 83 | `mkdir` | Implemented subset; see ABI/source |
| 92 | `chown` | Single UID/GID credential model |
| 99 | `sysinfo` | Implemented subset; see ABI/source |
| 102 | `getuid` | Implemented subset; see ABI/source |
| 105 | `setuid` | Implemented subset; see ABI/source |
| 106 | `setgid` | Implemented subset; see ABI/source |
| 107 | `geteuid` | Implemented subset; see ABI/source |
| 158 | `arch_prctl` | FS base subset |
| 186 | `gettid` | Implemented subset; see ABI/source |
| 202 | `futex` | Private WAIT/WAKE subset |
| 217 | `getdents64` | Implemented subset; see ABI/source |
| 218 | `set_tid_address` | Implemented subset; see ABI/source |
| 228 | `clock_gettime` | Implemented subset; see ABI/source |
| 231 | `exit_group` | Implemented subset; see ABI/source |
| 257 | `openat` | AT_FDCWD and relative directory FD |
| 262 | `newfstatat` | NOFOLLOW, EMPTY_PATH; NO_AUTOMOUNT accepted |
| 273 | `set_robust_list` | Robust mutex list registration |
| 274 | `get_robust_list` | Restricted task lookup |
| 280 | `utimensat` | Implemented subset; see ABI/source |
| 318 | `getrandom` | Implemented subset; see ABI/source |
| 324 | `membarrier` | QUERY = 0; no barrier commands |
| 435 | `clone3` | Thread flag combinations only |

## LOS extensions

| Number | Name | Scope |
| --- | --- | --- |
| 1024 | `display_info` | LOS-specific interface |
| 1025 | `display_present` | LOS-specific interface |
| 1026 | `display_present_rect` | LOS-specific interface |
| 1027 | `asset_publish` | LOS-specific interface |
| 1028 | `asset_acquire` | LOS-specific interface |
| 1029 | `asset_release` | LOS-specific interface |
| 1032 | `remove` | LOS file/directory removal extension |
| 1033 | `parent_chdir` | LOS privileged parent-shell cwd change |
| 1034 | `sudo_cache` | LOS authentication-session cache |
| 1035 | `system_monitor` | LOS-specific interface |
| 1036 | `ipc` | LOS copied messages; bounded queues |

## Unsupported behavior

Memfd_create (319) and sealing are unimplemented. Membarrier (324) supports capability query only. General signal delivery/handlers, job control, general poll waits, Linux socket syscalls, regular-file MAP_SHARED/writeback, O_PATH/O_TMPFILE and the complete Linux security/authentication API are not provided.

Unhandled numbers return ENOSYS and may emit an Unhandled syscall log. The inventory describes a subset, not Linux conformance. Build success, host execution and disposable-VM observations remain distinct.
