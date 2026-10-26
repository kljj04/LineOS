# User programs and libraries

[Documentation index](README.md) · [한국어](../ko_KR/UserLibraries.md) · [English](../en_US/UserLibraries.md)

## Deployment

Set `CopyPath=/usr/bin/` in `UserPrograms/<ProgramName>/Program.conf` to copy `LineOS/Programs/<ProgramName>.lix` into `/usr/bin/<ProgramName>.lix` on EXT2. The ELF64 contents and directory-derived filename are preserved. A missing configuration defaults to `/bin/`. Root `/` is allowed. Existing configurations require exactly one CopyPath key; blank lines, lines starting with `#`, and CRLF are supported. Configuration is parsed as data, never sourced.

Path components accept ASCII letters, digits, underscore, hyphen and dot. Relative paths, standalone dot/dot-dot components, repeated slashes, duplicate/unknown keys, empty values and configuration symlinks are rejected. Program names accept ASCII letters, digits, underscore and hyphen. Destination symlinks, nested mounts and existing destination hardlinks are rejected. Existing image formatting and mount lifecycle are preserved. Changing CopyPath does not delete the old copy. Deployment assumes the existing exclusive image-management workflow: the guest is stopped and no other process concurrently edits image contents.

## Userspace TrueType

`Libraries/Include/TrueType` and `Libraries/Source/TrueType` contain a user port of the kernel implementation. Make builds `Build/Libraries/libtruetype.a` and supplies it before libc on user link commands. Archive members are included only when referenced. The user library has no PMM, VirtIO GPU, kernel lock or kernel font-symbol dependencies. Kernel TrueType and panic output remain intact. The kernel console has been removed. The Terminal user program renders bytes received through its terminal master descriptor; ordinary programs use `read` and `write`.

Include `TrueType/TrueTypeEngine.h`. Supply a `TTF_ALLOCATOR` with aligned allocation/free callbacks and context, and a `TRUE_TYPE_SURFACE` with a tightly packed UINT32 BGRA8888 buffer of at least width * height * 4 bytes. `TrueTypeInit(&Allocator, &Surface)` configures the allocator and surface; it does not open files. Read each trusted TTF into memory, initialize a separate `TRUE_TYPE_FONT Font = {0}`, and call `TrueTypeRegisterFont(&Font, FontData)`. Pass `&Font` to the drawing and measurement functions. The font data, font object, allocator and surface must remain alive while used. The library does not select a fallback font automatically. Text uses CHAR16/UTF-16, including surrogate pairs. For direct FONT_INFO use, assign its UserData to a TTF_ALLOCATOR after InitFont. Packing APIs take that allocator as AllocContext.

Drawing changes CPU memory only. After checking the display dimensions, pitch and buffer capacity, the caller can use the existing `DisplayPresent(Surface.FrameBuffer, (UINT64) Surface.FrameBufferWidth * Surface.FrameBufferHeight * 4)`. The library does not invoke display syscalls or change kernel display ownership.

The engine holds one surface/allocator state per process; serialize calls. Fonts must be trusted: input-length validation and complete allocation-failure recovery are not provided. Existing mathematical approximations and single-line-oriented measurement behavior are retained. Terminal initializes its own library state and maps the font published by Init. Shell does not use TrueType or display APIs.

## Files and process creation

Image updates copy `Assets/Fonts/*.ttf` to EXT2 `/sys/fonts/` with their names and mode 0644. User programs can read them with `open(Path, O_RDONLY)` from `fcntl.h` and `read`/`close` from `unistd.h`. Each task has 16 descriptor slots, shared by files and terminal endpoints. Regular files currently support read-only opens. Errors return -1 and EOF returns 0; `errno` is not implemented. Reads resolve the path again, so POSIX open-file behavior across rename or deletion is not implemented. Access uses the VFS context supplied at execution; Init currently inherits an administrator context.

`spawn(Path)` from `spawn.h` starts a static ELF in a separate address space using the caller's VFS context, returning the child PID or -1 on failure. This is a simple LineOS extension, not POSIX `posix_spawn`. Children inherit references to the parent’s open objects before scheduling, sharing file offsets. Closing one descriptor preserves other references. Arguments, wait and exit-status collection are not yet supported. At most 16 child execution records are retained; terminated records are reclaimed on a later spawn once the scheduler has retired the task. Per-task SSE state preservation is not implemented; concurrent userspace floating-point/SIMD has not been validated. TrueType state and glyph caches remain per process; original TTF bytes are shared through the asset API below. Publishing authority is not inherited by children.

## Shared assets

Include `Asset.h`. All three functions return 0 on success or -1 on failure, without errno.

- `LOSAssetPublish(Name, Data, Size)` is restricted to the boot Init. It copies original bytes into kernel-owned pages once, allowing reuse of the source buffer and preserving assets after Init exits. Names contain 1–63 bytes. Assets are immutable; duplicate names and replacement are rejected.
- `LOSAssetAcquire(Name, &Asset)` maps those same physical pages read-only into the caller and returns `Data`, `Size`, and `Handle` in `LOS_ASSET`. Repeated acquisitions of a name in one process increment the same mapping's reference count. Use a separate structure and matching release for each acquisition. Copying a structure does not add a reference.
- `LOSAssetRelease(&Asset)` drops that process's reference, removes the mapping at the last reference, and clears the caller's structure. It preserves other processes' mappings and the published bytes. Address-space destruction cleans remaining mappings and their page tables without freeing the kernel-owned asset payload.

Limits are 16 published assets, 16MiB per asset, 64MiB total payload rounded to pages, and eight distinct mapped assets per process. Page tables and metadata are additional bounded allocations. The first version retains published bytes until reboot and has no unpublish, replacement or eviction API. Writes to mapped bytes cause protection exceptions. Consumers must stay within `Size`.

```c
LOS_ASSET Asset = {0};

if (LOSAssetAcquire("Fonts/JetBrainsMono", &Asset) == 0)
{
    TrueTypeRegisterFont(&Font, Asset.Data);
}
```

Keep the mapping alive until the font object and caches stop referencing its data. Only TTF bytes are shared: font objects, glyph caches, allocators and surfaces remain private. The asset API does not validate file formats.

## System call interface

The current `int 0x80` dispatch exposes exit, read, write, open, close, getpid, display information/presentation, spawn, and asset publication/acquisition/release. Yield hands the CPU to the scheduler and returns 0 on success. File and process interfaces expose only the subset described here; there is no `errno`. The former stream-read syscall number 10 is reserved and returns -1; `LOSOutputStreamRead` has been removed.

## Terminal and Shell

Init opens `/dev/lsh1` as descriptor 0 with `O_RDONLY` and descriptors 1 and 2 with `O_WRONLY`. It publishes the fonts, opens `/dev/lsh1/master` with `O_RDWR` as descriptor 3, and starts `/sbin/Terminal.lix`. It closes its master reference before starting `/bin/Shell.lix`, so Shell and its children inherit only the program endpoints. `Program.conf` installs Init and Terminal under `/sbin/`, and Shell under `/bin/`. The kernel starts `/sbin/Init.lix`.

Program-side `read` receives input and `write` sends output. Master-side `read` receives output and `write` injects input. Two independent 64 KiB queues bound storage. Empty reads and full writes sleep on wait queues; transferring bytes wakes the opposite side. Writes can return a partial count on a later copy or wait failure. Accepted output is mirrored to debugcon. Ordinary programs may open the program endpoint with `O_RDONLY`, `O_WRONLY` or `O_RDWR`. Only boot Init may open the master, and only one master object may exist at a time; its inherited references keep it alive. Final master close wakes waiters: input drains to EOF and new writes fail. Queue storage persists until reboot.

These device paths are recognized by `open`; a mounted devfs, directory enumeration, `stat` and general device registration are not implemented. The initial keyboard worker translates US-layout key events directly into this single terminal’s input queue. Shift, Caps Lock and alphabetic Ctrl combinations are supported; input is byte-oriented and a full hardware-input queue drops new keys. Future focus management can route input through the master endpoint.

Terminal owns the TrueType state, read-only JetBrains Mono mapping and framebuffer. It renders printable ASCII, newline, carriage return, backspace and scrolling, with 36-pixel text, font-derived horizontal advances and 40-pixel line spacing, on displays up to 2560 × 1440. It blocks reading descriptor 3 when output is absent. Shell uses only libc input/output and the LineOS `spawn` extension: it edits a bounded line, provides `help`, and launches an absolute executable path asynchronously. There is no foreground-job arbitration; multiple programs reading the same terminal compete for input.

The libc interface remains a limited freestanding subset. Standard function names do not imply full C or POSIX conformance. Pipes, descriptor duplication, `termios`, canonical mode, signals, job control, process waiting, UTF-8 terminal rendering and a general userspace allocator remain outside this implementation. Display and asset APIs remain LineOS extensions. User-memory copies and managed-address-space operations wait for the VMM lock while servicing pending TLB shootdowns; transient lock contention is not treated as an invalid user buffer or failed ELF mapping.
