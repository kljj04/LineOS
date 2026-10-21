# Storage and filesystems

[Documentation index](README.md) · [한국어](../ko_KR/Storage.md) · [English](../en_US/Storage.md)

- GPT partition lookup with header and partition-entry-array CRC validation
- Mounting the second GPT partition as a Linux EXT2 filesystem
- 1, 2, and 4 KiB blocks; revision 0/1; tested 128/256-byte inodes
- Superblock, block-group descriptor, block/inode bitmap, and inode handling
- Absolute path lookup, including relative and absolute symbolic links
- File and directory creation; file reads, writes, sparse extension, and truncation
- Direct, single-, double-, and triple-indirect block addressing
- File removal, empty-directory removal, and block/inode reclamation
- Same-volume rename, target replacement, and cross-directory moves
- Hard links and inline or block-backed symbolic links
- 32-bit UID/GID ownership and basic owner/group/other permission checks
- Sticky-directory checks and setgid directory inheritance
- Writable-mount validation of block ownership, allocation counts, directory references, and parent relationships
- Bounded in-memory preparation followed by dirty-state and flush-ordered disk updates

EXT2 is connected to a VFS core with per-partition roots, path resolution, directory enumeration, and command
contexts. `/` means the selected partition root; `^` is an administrator-only virtual partition manager. Normal users
receive a not-found result for that namespace. Administrator privileges do not add a prefix to ordinary `/A/B` paths.
There is no process file-descriptor table, open-file lifetime layer, or user-mode authentication system yet.

A single transaction can stage at most 32768 changed blocks (about 32–128 MiB including metadata). Oversized operations
fail before committing. Active
extended attributes/ACLs, supplementary groups, journaling, and unsupported features or inode flags are not supported
for modification. EXT2 has no journal here: dirty marking and flush ordering do not guarantee atomic recovery or data
preservation after real power loss. A failed disk commit blocks further access through that mount and requires offline
filesystem checking before the next writable mount.

The public filesystem entry points are declared in `Kernel/Include/FS/EXT2/EXT2.h`.

## VFS integration and clock

VFS and the EXT2 adapter are implemented under `Kernel/Source/FS/VFS/`. They provide partition-root namespaces, path resolution and command contexts. Public interfaces are under `Kernel/Include/FS/`.

`ClockGetUnixTime()` supplies RTC-backed UTC Unix time. EXT2 uses ctime for inode changes, mtime for content changes and relatime atime updates on writable mounts. Local display offsets do not change disk timestamps.

The former kernel development shell is not part of the current `KMain()` workload. A userland shell, authentication, process file descriptors and open-file lifetime management remain future work. The EFI FAT32 partition is used by the host launcher for boot files; it does not imply a kernel FAT32 filesystem backend.

See [verification](Testing.md) for historical EXT2 results and their limits.
