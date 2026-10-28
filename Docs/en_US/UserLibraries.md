# User programs and libraries

[Documentation index](README.md) · [한국어](../ko_KR/UserLibraries.md) · [English](../en_US/UserLibraries.md)

## Deployment

Set `CopyPath=/usr/bin/` in `UserPrograms/<ProgramName>/Program.conf` to copy `LineOS/Programs/<ProgramName>.lix` into
`/usr/bin/<ProgramName>.lix` on EXT2. The ELF64 contents and directory-derived filename are preserved. A missing
configuration defaults to `/bin/`. Root `/` is allowed. Existing configurations require exactly one CopyPath key; blank
lines, lines starting with `#`, and CRLF are supported. Configuration is parsed as data, never sourced.

Path components accept ASCII letters, digits, underscore, hyphen and dot. Relative paths, standalone dot/dot-dot
components, repeated slashes, duplicate/unknown keys, empty values and configuration symlinks are rejected. Program
names accept ASCII letters, digits, underscore and hyphen. Destination symlinks, nested mounts and existing destination
hardlinks are rejected. Existing image formatting and mount lifecycle are preserved. Changing CopyPath does not delete
the old copy. Deployment assumes the existing exclusive image-management workflow: the guest is stopped and no other
process concurrently edits image contents.

## Userspace TrueType

`Libraries/Include/TrueType` and `Libraries/Source/TrueType` contain a user port of the kernel implementation. Make
builds `Build/Libraries/libtruetype.a` and links it for programs listing `TrueType` in `Program.cfg` under `Dependencies`. Archive members are included
only when referenced. The user library has no PMM, VirtIO GPU, kernel lock or kernel font-symbol dependencies. Kernel
TrueType and panic output remain intact. The kernel console has been removed. The Terminal user program renders bytes
received through its terminal master descriptor; ordinary programs use `read` and `write`.

Include `TrueType/TrueTypeEngine.h`. Supply a `TTF_ALLOCATOR` with aligned allocation/free callbacks and context, and a
`TRUE_TYPE_SURFACE` with a tightly packed UINT32 BGRA8888 buffer of at least width * height * 4 bytes.
`TrueTypeInit(&Allocator, &Surface)` configures the allocator and surface; it does not open files. Read each trusted TTF
into memory, initialize a separate `TRUE_TYPE_FONT Font = {0}`, and call `TrueTypeRegisterFont(&Font, FontData)`. Pass
`&Font` to the drawing and measurement functions. The font data, font object, allocator and surface must remain alive
while used. The library does not select a fallback font automatically. Text uses CHAR16/UTF-16, including surrogate
pairs. For direct FONT_INFO use, assign its UserData to a TTF_ALLOCATOR after InitFont. Packing APIs take that allocator
as AllocContext.

Drawing changes CPU memory only. After checking the display dimensions, pitch and buffer capacity, the caller can use
the existing `DisplayPresent(Surface.FrameBuffer, (UINT64) Surface.FrameBufferWidth * Surface.FrameBufferHeight * 4)`.
The library does not invoke display syscalls or change kernel display ownership.

The engine holds one surface/allocator state per process; serialize calls. Fonts must be trusted: input-length
validation and complete allocation-failure recovery are not provided. Existing mathematical approximations and
single-line-oriented measurement behavior are retained. Terminal initializes its own library state and maps the font
published by Init. Shell does not use TrueType or display APIs.

## Files and process creation

Image updates copy `Assets/Fonts/*.ttf` to EXT2 `/sys/fonts/` with their names and mode 0644. User programs can read
them with `open(Path, O_RDONLY)` from `fcntl.h` and `read`/`close` from `unistd.h`. Each task has 16 descriptor slots,
shared by files and terminal endpoints. Regular files support read/write/create/truncate using the documented flags. Errors return -1 with `errno`; EOF returns 0. Reads resolve the path again, so POSIX open-file behavior across rename or
deletion is not implemented. Access uses the VFS context supplied at execution; Init currently inherits an administrator
context.

`fork()` duplicates the caller and returns its child PID to the parent and zero to the child. The child calls `execve(Path, Argv, Envp)` to replace its image, keeping PID, inherited file descriptions, credentials and cwd. Init, Login, UserRescue and Shell all use this sequence. The spawn/spawnv extension is removed. Exec failure returns -1 with errno in the child; launch sites report it and exit with 127. waitpid/wait collect child exit status; Shell waits for its external child before printing the next prompt. At most 16 child execution records are retained and terminated records are reclaimed during later fork calls. Publishing authority is not inherited by fork children. Per-task SSE state preservation remains unimplemented; concurrent floating-point/SIMD is not validated. TrueType state and caches remain private; original font assets remain shared.

## Shared assets

Include `Asset.h`. All three functions return 0 on success or -1 on failure, with `errno` on failure.

- `LOSAssetPublish(Name, Data, Size)` is restricted to the boot Init. It copies original bytes into kernel-owned pages
  once, allowing reuse of the source buffer and preserving assets after Init exits. Names contain 1–63 bytes. Assets are
  immutable; duplicate names and replacement are rejected.
- `LOSAssetAcquire(Name, &Asset)` maps those same physical pages read-only into the caller and returns `Data`, `Size`,
  and `Handle` in `LOS_ASSET`. Repeated acquisitions of a name in one process increment the same mapping's reference
  count. Use a separate structure and matching release for each acquisition. Copying a structure does not add a
  reference.
- `LOSAssetRelease(&Asset)` drops that process's reference, removes the mapping at the last reference, and clears the
  caller's structure. It preserves other processes' mappings and the published bytes. Address-space destruction cleans
  remaining mappings and their page tables without freeing the kernel-owned asset payload.

Limits are 16 published assets, 16MiB per asset, 64MiB total payload rounded to pages, and eight distinct mapped assets
per process. Page tables and metadata are additional bounded allocations. The first version retains published bytes
until reboot and has no unpublish, replacement or eviction API. Writes to mapped bytes cause protection exceptions.
Consumers must stay within `Size`.

```c
LOS_ASSET Asset = {0};

if (LOSAssetAcquire("Fonts/JetBrainsMono", &Asset) == 0)
{
    TrueTypeRegisterFont(&Font, Asset.Data);
}
```

Keep the mapping alive until the font object and caches stop referencing its data. Only TTF bytes are shared: font
objects, glyph caches, allocators and surfaces remain private. The asset API does not validate file formats.

## System call interface

The current `syscall`/`sysretq` dispatch exposes exit, read, write, open, close, getpid, display information/presentation, fork/execve, chdir, stat/lstat/fstat,
and asset publication/acquisition/release. Yield hands the CPU to the scheduler and returns 0 on success. File and
process interfaces expose the subset described here. Raw calls return Linux negative errors; libc returns -1 and sets `errno` for errors in -4095..-1. `LOSOutputStreamRead` has been removed. Number 10 is reserved for Linux `mprotect` and is currently unsupported.

`RAX` carries the syscall number and result. Arguments use `RDI`, `RSI`, `RDX`, `R10`, `R8`, and `R9`.
`RCX` and `R11` are clobbered. Each CPU configures the entry MSRs and switches to the current task's kernel stack.
The former user-callable `int 0x80` gate is removed. Standard calls use Linux x86-64 syscall numbers. LineOS extensions start at 1024; display and asset calls remain available. Device `ioctl`, anonymous private `mmap`, and `/dev/fb0` shared `mmap` are implemented; see [DeviceFS and process ABI](DeviceFS.md).

| Call | Number |
| --- | --- |
| `read` | 0 |
| `write` | 1 |
| `open` | 2 |
| `close` | 3 |
| `stat` | 4 |
| `fstat` | 5 |
| `lstat` | 6 |
| `mmap` | 9 |
| `munmap` | 11 |
| `ioctl` | 16 |
| `sched_yield` | 24 |
| `getpid` | 39 |
| `fork` | 57 |
| `execve` | 59 |
| `exit` | 60 |
| `wait4` | 61 |
| `getcwd` | 79 |
| `chdir` | 80 |
| `rename` | 82 |
| `mkdir` | 83 |
| `setuid` | 105 |
| `setgid` | 106 |
| `getdents64` | 217 |
| `getrandom` | 318 |
| `display_info` | 1024 |
| `display_present` | 1025 |
| `display_present_rect` | 1026 |
| `asset_publish` | 1027 |
| `asset_acquire` | 1028 |
| `asset_release` | 1029 |
| `remove (LineOS)` | 1032 |

`remove` remains a LineOS extension because it handles both files and directories. It is not Linux `unlink` or `rmdir`.
Matching supported syscall numbers, error values and framebuffer query structures does not provide full Linux binary compatibility. See [supported features and limits](DeviceFS.md).

## Terminal and Shell

Init opens `/dev/lsh1` as descriptor 0 with `O_RDONLY` and descriptors 1 and 2 with `O_WRONLY`. It publishes the fonts,
opens `/dev/lsh1-master` with `O_RDWR` as descriptor 3, and starts `/sbin/Terminal.lix`. It closes its master reference
before starting `/bin/Shell.lix`, so Shell and its children inherit only the program endpoints. `Program.cfg` installs
Init and Terminal under `/sbin/`, and Shell under `/bin/`. The kernel starts `/sbin/Init.lix`.

Program-side `read` receives input and `write` sends output. Master-side `read` receives output and `write` injects
input. Two independent 64 KiB queues bound storage. Empty reads and full writes sleep on wait queues; transferring bytes
wakes the opposite side. Writes can return a partial count on a later copy or wait failure. Accepted output is mirrored
to debugcon. Ordinary programs may open the program endpoint with `O_RDONLY`, `O_WRONLY` or `O_RDWR`. Only boot Init may
open the master, and only one master object may exist at a time; its inherited references keep it alive. Final master
close wakes waiters: input drains to EOF and new writes fail. Queue storage persists until reboot.

DeviceFS now provides these nodes through VFS and supports directory enumeration with `getdents64`. `stat/lstat/fstat` report device metadata; dynamic device registration remains unsupported. The initial keyboard worker translates US-layout key events directly into this single
terminal’s input queue. Shift, Caps Lock and alphabetic Ctrl combinations are supported; input is byte-oriented and a
full hardware-input queue drops new keys. Future focus management can route input through the master endpoint.

Terminal owns the TrueType state, read-only JetBrains Mono mapping and framebuffer. It renders printable ASCII, newline,
carriage return, backspace and scrolling, with 36-pixel text, font-derived horizontal advances and 40-pixel line
spacing, on displays up to 2560 × 1440. It blocks reading descriptor 3 when output is absent. Shell uses only libc
input/output, fork/execve and chdir: it edits a bounded line, provides help and cd [PATH], and launches relative or absolute
executable paths and waits for completion. It has no built-in ls. There is no foreground-job arbitration; multiple programs reading the same terminal
compete for input.

The libc interface remains a limited freestanding subset. Standard function names do not imply full C or POSIX
conformance. Pipes, descriptor duplication, `termios`, canonical mode, signals, job control and a general userspace allocator remain outside this implementation. Display and asset APIs remain
LineOS extensions. User-memory copies and managed-address-space operations wait for the VMM lock while servicing pending
TLB shootdowns; transient lock contention is not treated as an invalid user buffer or failed ELF mapping.

## Terminal ANSI colors

Terminal parses SGR colors across read boundaries: 30–37/40–47, 90–97/100–107, 38;5;N / 48;5;N (256 colors), and 38;2;R;G;B / 48;2;R;G;B (all 24-bit RGB values). SGR 0 or an empty parameter resets attributes; 39/49 restore default foreground/background. SGR 1/22 toggles bright basic foreground colors, and 7/27 toggles inverse. Backgrounds fill each font-metric cell, including spaces; scrolling preserves rendered pixels and fills exposed rows with the current background.

For example, `printf("\033[38;2;145;231;57mColor\033[0m\n");` sets a truecolor foreground. The parser bounds parameter count and numeric values, consumes unsupported CSI commands and OSC strings, and does not print their syntax. Cursor positioning, screen erase, underline, italics and other terminal-emulator features are not implemented by this color extension. Existing UTF-8, fonts, input, cursor and shared-framebuffer presentation remain. See [XTerm control sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html) for SGR conventions.

The prompt uses the original heavy TTF glyphs ┏━ and ┗━>. Background clearing uses the actual font ascent and line height, preserving connecting glyph pixels outside the current row without replacing or stretching the glyph shape. Horizontal advance remains font-derived.

Only the first command word is highlighted: exact builtins help/cd/pwd/echo use RGB(214,122,122); existing executable files resolved through the same PATH lookup as execution use RGB(128,184,136). Missing or unresolved commands use white RGB(255,255,255) on RGB(255,95,95) background. Arguments remain default white. Classification changes redraw through CR/CSI 2 K; normal typing and IME remain incremental, and output resets attributes. External arguments are split on spaces, with at most 64 arguments; quoting and escape parsing are not implemented.
