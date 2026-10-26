# User programs and libraries

[Documentation index](README.md) · [한국어](../ko_KR/UserLibraries.md) · [English](../en_US/UserLibraries.md)

## Deployment

Set `CopyPath=/usr/bin/` in `UserPrograms/<ProgramName>/Program.conf` to copy `LineOS/Programs/<ProgramName>.lix` into `/usr/bin/<ProgramName>.lix` on EXT2. The ELF64 contents and directory-derived filename are preserved. A missing configuration defaults to `/bin/`. Root `/` is allowed. Existing configurations require exactly one CopyPath key; blank lines, lines starting with `#`, and CRLF are supported. Configuration is parsed as data, never sourced.

Path components accept ASCII letters, digits, underscore, hyphen and dot. Relative paths, standalone dot/dot-dot components, repeated slashes, duplicate/unknown keys, empty values and configuration symlinks are rejected. Program names accept ASCII letters, digits, underscore and hyphen. Destination symlinks, nested mounts and existing destination hardlinks are rejected. Existing image formatting and mount lifecycle are preserved. Changing CopyPath does not delete the old copy. Deployment assumes the existing exclusive image-management workflow: the guest is stopped and no other process concurrently edits image contents.

## Userspace TrueType

`Libraries/Include/TrueType` and `Libraries/Source/TrueType` contain a user port of the kernel implementation. Make builds `Build/Libraries/libtruetype.a` and supplies it before libc on user link commands. Archive members are included only when referenced. The user library has no PMM, VirtIO GPU, kernel lock or kernel font-symbol dependencies. Kernel TrueType and panic output remain intact. The kernel console has been removed. `write` appends to a temporary global 64 KiB output buffer, read by Shell through `LOSOutputStreamRead`. A full buffer causes a partial write or failure; an empty read fails.

Include `TrueType/TrueTypeEngine.h`. Supply a `TTF_ALLOCATOR` with aligned allocation/free callbacks and context, and a `TRUE_TYPE_SURFACE` with a tightly packed UINT32 BGRA8888 buffer of at least width * height * 4 bytes. `TrueTypeInit(&Allocator, &Surface)` configures the allocator and surface; it does not open files. Read each trusted TTF into memory, initialize a separate `TRUE_TYPE_FONT Font = {0}`, and call `TrueTypeRegisterFont(&Font, FontData)`. Pass `&Font` to the drawing and measurement functions. The font data, font object, allocator and surface must remain alive while used. The library does not select a fallback font automatically. Text uses CHAR16/UTF-16, including surrogate pairs. For direct FONT_INFO use, assign its UserData to a TTF_ALLOCATOR after InitFont. Packing APIs take that allocator as AllocContext.

Drawing changes CPU memory only. After checking the display dimensions, pitch and buffer capacity, the caller can use the existing `DisplayPresent(Surface.FrameBuffer, (UINT64) Surface.FrameBufferWidth * Surface.FrameBufferHeight * 4)`. The library does not invoke display syscalls or change kernel display ownership.

The engine holds one surface/allocator state per process; serialize calls. Fonts must be trusted: input-length validation and complete allocation-failure recovery are not provided. Existing mathematical approximations and single-line-oriented measurement behavior are retained. Shell initializes its own library state; Init only publishes assets. A QEMU/KVM single-CPU run displayed the Shell output; that observation does not establish multi-CPU correctness.

## Files and process creation

Image updates copy `Assets/Fonts/*.ttf` to EXT2 `/sys/fonts/` with their names and mode 0644. User programs can read them with `open(Path, O_RDONLY)` from `fcntl.h` and `read`/`close` from `unistd.h`. There are eight read-only descriptors per task. Errors return -1 and EOF returns 0; `errno` is not implemented. Reads resolve the path again, so POSIX open-file behavior across rename or deletion is not implemented. Access uses the VFS context supplied at execution; Init currently inherits an administrator context.

`spawn(Path)` from `spawn.h` starts a static ELF in a separate address space using the caller's VFS context, returning the child PID or -1 on failure. This is a simple LineOS extension, not POSIX `posix_spawn`. Arguments, wait, exit-status collection and file-descriptor inheritance are not yet supported. At most 16 child execution records are retained; terminated records are reclaimed on a later spawn once the scheduler has retired the task. Per-task SSE state preservation is not implemented; concurrent userspace floating-point/SIMD has not been validated. TrueType state and glyph caches remain per process; original TTF bytes are shared through the asset API below. Publishing authority is not inherited by children.

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

The current `int 0x80` dispatch exposes exit, read, write, open, close, getpid, display information/presentation, output stream reading, spawn, and asset publication/acquisition/release. The yield number is reserved but currently returns failure. File and process interfaces expose only the subset described here; there is no `errno` or general descriptor inheritance.

## Output and Shell

The libc `printf`, `puts` and `putchar` use `write` on stdout. The kernel accepts writes to descriptors 1 and 2, copies bytes to the global stream, and currently mirrors attempted bytes to debugcon. The stream is shared and destructive: readers consume the same bytes, with no per-process routing or blocking read. It is not a terminal, pipe, or a complete POSIX stdio implementation. The diagnostic mirror may include bytes that do not fit in the stream.

Init reads Pretendard and JetBrains Mono once, publishes `Fonts/Pretendard` and `Fonts/JetBrainsMono`, starts `/bin/Shell.lix`, and exits. Shell maps JetBrains Mono read-only and initializes its own TrueType surface, allocator and font object. It consumes stream bytes, renders printable ASCII with newline, carriage return and scrolling, and presents the framebuffer. It has no keyboard input or command interpreter. Displays up to 2560 × 1440 are supported. It polls an empty stream; yield currently returns -1, so it can consume CPU.

The libc interface is a limited freestanding subset. Standard names such as `open`, `read`, `write` and `printf` do not imply full C or POSIX conformance. `spawn` and the display and stream interfaces are LineOS-specific; `exec`, process waiting, a general userspace allocator and terminal input are not implemented.
