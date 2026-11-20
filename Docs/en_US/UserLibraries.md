# Userspace programs and libraries

[Documentation index](README.md) · [한국어](../ko_KR/UserLibraries.md) · [English](../en_US/UserLibraries.md)

## Runtime layout

Bundled programs use project musl for stdio, allocation, strings, pthread, errno and TLS. LOSLib supplies LOS-specific syscall APIs. Host Ubuntu glibc is not the default runtime. Executables use musl Scrt1.o/crti.o/crtn.o, entry `_start`, and default PT_INTERP `/lib/libc.so`.

| Logical name | Output | Responsibility |
| --- | --- | --- |
| musl | libc.so | libc and dynamic interpreter |
| LOSLib | liblos.so | Assets, display, IPC, monitoring and LOS extensions |
| Window | libwindow.so | IPC window-client helpers |
| Locale | liblocale.so | Built-in translations and LOS locale/timezone defaults |
| TrueType | libtruetype.so | Userspace font parsing, measurement and rasterization |
| UserDB | libuserdb.so | LOS accounts, Argon2id authentication and account management |
| VirGL | libvirgl.so | LOS VirGL device access |

Loader has a separate dedicated build producing `ld-lineos-x86_64.so.1`; it does not replace the default musl interpreter. A VirGL device API does not imply a complete Mesa OpenGL/EGL implementation.

## Configuration and automation

[Program.cfg](ProgramConfig.md) declares executable names, installation, libraries, assets, publication and interpreter. [Library.cfg](LibraryConfig.md) declares shared-library builds, deployment, libc and library dependencies. Discovery is automatic; adding ordinary programs/libraries requires no common Makefile edits.

LibC=musl automatically adds musl. Library LibC=none selects freestanding compilation; programs do not yet support none. Programs currently include LOSLib as a default link input; asset use/publication also adds it to the declared dependency list. Other directly used libraries must be declared in `[Dependencies] Libraries`; include statements and function calls are not scanned. Transitive public headers, build ordering and search paths follow the declared graph. `--as-needed` removes unused link inputs.

Choose generic Source/ compilation or a custom `[Build] Makefile` plus Artifact. Outputs are staged under LineOS/Libraries and deployed using CopyPath. Programs and generic libraries record guest dependency directories in RUNPATH. Custom Makefiles own their SONAME, RUNPATH and compiler settings.

## Assets and fonts

Init's `[Publish] Assets` publishes `/sys/fonts/JBMNFSB.ttf` and `/sys/fonts/PTDSB.ttf` as Fonts/JetBrainsMono and Fonts/Pretendard. Generated constructors read/publish files and then validate required assets before main. The kernel stores read-only shared bytes; consumers manage mapping lifetimes through LOSAssetAcquire/Release. Configuration does not grant publisher privileges.

TrueType consumes shared bytes, but font objects, glyph caches, allocators and rendering buffers remain process-private. Kernel panic fonts are separate. Public interfaces are in Libraries/TrueType/Include and implementations in Source.

## Startup and interaction

The kernel starts `/sbin/Init.lix`. Init opens the TTY and starts Terminal and Login. Login authenticates through UserDB, applies UID/GID and home, then starts user autostart entries and `/bin/lsh/lsh.lix`. The default autostart is `/sys/lwm/lwm.lix`; existing user configuration is preserved.

Terminal owns master output, fonts and framebuffer rendering, handling UTF-8, ANSI colors, cursor operations and screen updates. When lwm owns the screen, Terminal sends IPC background frames and forwards input to the shell. See [lwm](Lwm.md), [IPC](IPC.md) and [autostart](Autostart.md) for actual GUI/session limits.

Lsh supports interactive input, quoting/expansion, variables, history, path lookup and fork/execve/wait. Its two-line prompt uses `┏━[PATH]━(USER)` and `┗━>`. Stdout is unbuffered to preserve prompt/ANSI cursor ordering; Login and sudo flush input prompts before reads. See [shell variables](ShellVariables.md). This is not a complete POSIX shell: pipes, job control and ordinary signal delivery are not provided.

## Accounts, translation and limits

[Users and sudo](Users.md) describes custom authentication versus standard account lookup. [Locale](Locale.md) describes file defaults, environment precedence and built-in translations. See [DeviceFS/ABI](DeviceFS.md), the [syscall inventory](Syscalls.md) and [validation guide](Validation.md) for kernel limits and test procedures.

## TrueType API and lifetimes

Include `<TrueTypeEngine.h>` and provide a TTF_ALLOCATOR with allocation/free callbacks and context, plus a TRUE_TYPE_SURFACE backed by a tightly packed 32-bit BGRA8888 buffer. TrueTypeInit configures state without opening files. Zero-initialize each TRUE_TYPE_FONT, register its bytes with TrueTypeRegisterFont, then use DrawTrueTypeCodepoint, DrawTrueTypeText or MeasureTrueTypeText. Strings are CHAR16/UTF-16 with surrogate handling. Font bytes, objects, allocator and surface must remain alive.

Drawing updates CPU memory only; check display ownership, pitch and capacity before presenting through DisplayPresent, framebuffer or window APIs. The engine keeps one surface/allocator state per process; serialize calls. Font input must be trusted: complete length validation, allocation-failure recovery and automatic fallback selection are not provided. Lsh can measure advances through this library; Terminal renders glyphs.

## Asset limits and ownership

The kernel has 16 publication slots, a 64 MiB total page budget, a 16 MiB per-asset limit and names up to 63 bytes excluding NUL. Each process can map eight distinct assets; repeated acquisition increments references. Release drops a process mapping reference, not the publication. Unpublication, replacement and format validation are unsupported. Published names and disk paths are separate namespaces.
