# LOSLib

`liblos.so` supplies LineOS-specific syscall wrappers: Display, Asset, IPC, system monitoring, remove, parent-directory
change and sudo credential caching. Public headers are in `Include/`; shared ABI types are in `../../Common/Userspace`
and
`Common/Syscall/`.

The library uses the bundled musl `errno` and standard helper functions. It does not define libc, allocation, stdio,
string, POSIX file/process/time APIs, pthreads, errno storage, TLS or program startup. Raw syscall helpers are internal
hidden symbols.

Applications use musl headers, CRT and `/lib/libc.so` as interpreter. `liblos.so` records `libc.so` as a dependency.
Window convenience APIs reside in `Libraries/Window`; project translations and locale/timezone defaults reside in
`Libraries/Locale`, with `LOSGettext` separate from musl's standard gettext API.

After building, run `bash Scripts/Validation/LOSLib.sh` for export/dependency checks, musl errno integration, and host
execution of hello/pthreadtest. `Scripts/Validation/LOSLib/Guest.c` also provides a boot-Init fixture: link it with musl
CRT, `liblos.so` and `libc.so`, using `/lib/libc.so` as interpreter. Deploy only to a disposable image. It writes
`/loslib-musl-PASS` with an eight-byte zero on success, or `/loslib-musl-FAIL` with the failing source line. A 2-vCPU
KVM run verified private file mapping, malloc/TLS/errno and LOS Asset/IPC calls.

Kernel syscall support remains a defined subset. Root-owned setuid dynamic execution now uses AT_SECURE and trusted-path
checks; setgid execution is unsupported. See [users and sudo](../../Docs/en_US/Users.md) and
the [ABI inventory](../../Docs/en_US/Syscalls.md). Locale defaults use the existing fixed timezone table, without
introducing a DST database. The old LineOS interpreter is retained for standalone loader validation.
