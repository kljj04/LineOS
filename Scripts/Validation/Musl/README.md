# musl runtime fixture

Run `bash Scripts/Validation/Musl.sh` from the repository. It builds the existing hello command through `Dependencies = musl`, then builds `Build/Validation/Musl/Test.lix` with musl headers and startup objects. Both programs must record `/lib/libc.so` as interpreter and `libc.so` as a dependency, without LOSLibC.

For a boot test, use a disposable disk copy. Deploy the current kernel to its EFI partition, `LineOS/Libraries/libc.so` to `/lib/libc.so`, the built hello command to `/bin/lsh/hello.lix`, and the fixture to `/sbin/Init.lix` in that copy. The fixture opens the terminal master as boot Init, so it is not intended to run as an ordinary shell command. Do not replace Init on the normal image.

The fixture tests writev (including invalid vector count and inaccessible user buffers), anonymous PROT_NONE mappings, mprotect restoration without losing data, fork preservation of protected pages, munmap, malloc/free, compiler TLS, formatted output, missing-file errno, and fork/exec/wait of the real hello command. Success requires both `Hello World!` and `MUSL: ALL PASS` in debugcon output; guest exit alone is insufficient.

Verified on a copied EXT2 disk with 2-vCPU KVM. This does not validate pthreads, dlopen, ordinary file-backed mmap, MAP_FIXED, or general musl/POSIX compatibility. The normal disk image is updated separately through the existing builder.
