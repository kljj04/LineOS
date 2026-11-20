# musl runtime fixture

Run `bash Scripts/Validation/Musl.sh` from the repository. It builds the existing hello command with the default musl runtime, then builds `Build/Validation/Musl/Test.lix` with musl headers and startup objects. Both programs must record `/lib/libc.so` as interpreter and `libc.so` as a dependency, and LOSLib only when the current hello source/configuration actually uses LOS APIs.

For a boot test, use a disposable disk copy. Deploy the current kernel to its EFI partition, `LineOS/Libraries/libc.so` to `/lib/libc.so`, the built hello command to `/bin/lsh/hello.lix`, and the fixture to `/sbin/Init.lix` in that copy. The fixture opens the terminal master as boot Init, so it is not intended to run as an ordinary shell command. Do not replace Init on the normal image.

The fixture tests writev (including invalid vector count and inaccessible user buffers), anonymous PROT_NONE mappings, mprotect restoration without losing data, fork preservation of protected pages, munmap, malloc/free, compiler TLS, formatted output, missing-file errno, and fork/exec/wait of the real hello command. Success requires both `Hello World!` and `MUSL: ALL PASS` in debugcon output; guest exit alone is insufficient.

Verified on a copied EXT2 disk with 2-vCPU KVM. That historical fixture did not validate pthreads, dlopen, ordinary file-backed mmap or MAP_FIXED. Later implementations and dedicated tests are documented in [the validation guide](../../../Docs/en_US/Validation.md); none of these tests establishes general musl/POSIX compatibility. The normal disk image is updated separately through the existing builder.

현재 fixture는 Init 설정에서 생성한 게시 초기화 코드도 링크합니다. 사본 이미지에 Init의 `[Publish]` 원본 폰트 파일을 배치해야 하며, hello에 에셋 의존성을 선언한 경우에만 실행 전에 검사합니다.
