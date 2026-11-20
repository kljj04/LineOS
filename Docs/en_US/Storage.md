# Storage and filesystems

[Documentation index](README.md) · [한국어](../ko_KR/Storage.md) · [English](../en_US/Storage.md)

## GPT and EXT2

GPT header and partition-array CRCs are checked before mounting partition 2 as EXT2. The backend supports revision 0/1, 1/2/4 KiB blocks and 128/256-byte inodes.

Files and directories support creation, read/write, sparse extension, truncation, removal, same-volume rename/moves and target replacement, hard links and inline/block symbolic links. File addressing supports direct, single-, double- and triple-indirect blocks.

Permissions use 32-bit UID/GID, owner/group/other checks, sticky directories and setgid directory inheritance. Writable mounts validate ownership, allocation counts, directory references and parent relationships.

## VFS and timestamps

EXT2 is connected to a VFS core with per-partition roots, path resolution, directory enumeration, and command contexts. `/` means the selected partition root; `^` is an administrator-only virtual partition manager. Normal users receive a not-found result for that namespace. Administrator privileges do not add a prefix to ordinary `/A/B` paths. Processes have read/write descriptors and shared open-file objects; Login and sudo provide userspace authentication. Full inode-backed lifetime across rename/unlink remains unsupported.

`ClockGetUnixTime()` supplies RTC-backed UTC Unix time. EXT2 uses ctime for inode changes, mtime for content changes and relatime atime updates on writable mounts. Local display offsets do not change disk timestamps.

## Write failures and limits

A single transaction can stage at most 32768 changed blocks (about 32–128 MiB including metadata). Oversized operations fail before committing. Active extended attributes/ACLs, supplementary groups, journaling, and unsupported features or inode flags are not supported for modification. EXT2 has no journal here: dirty marking and flush ordering do not guarantee atomic recovery or data preservation after real power loss. A failed disk commit blocks further access through that mount and requires offline filesystem checking before the next writable mount.

The public filesystem entry points are declared in `Kernel/Include/FS/EXT2/EXT2.h`.

## Sources and startup

VFS and the EXT2 adapter are implemented under `Kernel/Source/FS/VFS/`. They provide partition-root namespaces, path resolution and command contexts. Public interfaces are under `Kernel/Include/FS/`.

The former kernel development shell is absent; the current userspace lsh is interactive through Terminal. `KMain()` initializes the block device and mounts GPT partition 2 as the EXT2 VFS root. The host image updater also deploys configured programs, shared libraries and fonts to EXT2; see [building and running](Build.md). The host use of the EFI FAT32 partition does not imply a kernel FAT32 backend.

## Open-file identity

EXT2 i_generation is carried through VFS nodes and open files and changes when an inode is reused. Namespace-wide generation remains a directory-mutation counter, not an FD validity token. Unrelated file creation/removal therefore does not invalidate open files or interrupt shared-library loading.
